//! nipovpn — HTTP-obfuscation proxy. Rust port of the C++ implementation.
//! CLI: `nipovpn <server|agent> <config.yaml>`.
//!
//! # What it does
//!
//! nipovpn hides real traffic by disguising it as ordinary HTTP. It runs as two
//! cooperating processes (same binary, different `mode` arg, usually the same
//! `config.yaml`):
//!
//! - **agent** — runs next to the client. Accepts the client's proxy protocol
//!   (SOCKS5 or HTTP-proxy), encrypts the request, and wraps it inside a *fake*
//!   HTTP POST sent to the server.
//! - **server** — runs on the exit side. Unwraps + decrypts, performs the real
//!   request to the target host, encrypts the reply, and sends it back.
//!
//! ```text
//!   client ──SOCKS5/HTTP──▶ AGENT ──fake HTTP(+TLS)──▶ SERVER ──plain──▶ target
//!          ◀──────────────       ◀───────────────────        ◀─────────
//! ```
//!
//! # Request lifecycle (follow the code)
//!
//! 1. [`server`] binds the listen socket and `tokio::spawn`s one task per
//!    accepted connection. In server mode it first does the TLS handshake.
//! 2. [`connection::Connection`] drives that task:
//!    - agent: [`connection::Connection::run_agent`] — optional SOCKS5
//!      handshake ([`connection`] `socks5_handshake`), then a request loop.
//!    - server: [`connection::Connection::run_server`] — a request loop.
//! 3. Per request the driver calls a handler:
//!    - [`agent_handler::handle`] — `hex(raw)` → [`crypto::aes256_encrypt`] →
//!      [`crypto::encode64`], placed in a fake outer POST (random method / URL /
//!      endpoint from [`config`]), sent over the persistent link in [`client`].
//!    - [`server_handler::handle`] — reverse: [`crypto::decode64`] →
//!      [`crypto::aes256_decrypt`], then dispatch on the `X-Nipo-Action` header.
//! 4. [`http`] classifies the inner request (HTTP vs TLS ClientHello, extracts
//!    SNI / host:port) so the server knows the real target.
//!
//! # CONNECT tunnels (two strategies, chosen by `tunnelEnable`)
//!
//! A CONNECT request opens a raw byte tunnel (e.g. browser HTTPS). Either:
//!
//! - **direct tunnel** (`tunnelEnable: true`): after the 200, both sides switch
//!   to `tokio::io::copy_bidirectional`, relaying raw bytes over the single
//!   agent↔server link. Lowest latency.
//! - **poll tunnel** (`tunnelEnable: false`): the agent ships tunnel bytes as
//!   discrete `send` requests and pulls return bytes with periodic `recv`
//!   requests (`X-Nipo-Action`); a final `close` tears it down. The server keeps
//!   the target socket in a session map ([`server_handler::Sessions`]) keyed by
//!   `X-Nipo-Session`. Looks like normal request/response HTTP on the wire.
//!
//! # Wire format (must stay byte-compatible with the C++ build)
//!
//! - Crypto: AES-256-CBC, random 16-byte IV **prepended** to ciphertext, PKCS7.
//! - Agent→server body is base64; server→agent body is raw ciphertext (asymmetry
//!   is intentional — see [`agent_handler`] / [`server_handler`]).
//! - Inner client bytes are hex-encoded *before* encryption (see [`crypto`]).
//!
//! # Module map
//!
//! | Module | Role |
//! |---|---|
//! | [`config`] | YAML config, defaults, validation, random fake-URL/method/endpoint |
//! | [`crypto`] | AES-256-CBC, base64, hex — the wire-critical transforms |
//! | [`http`] | HTTP parse + TLS-ClientHello SNI extraction + header helpers |
//! | [`tls`] | rustls server acceptor / client connector (SNI, ALPN, verify-none) |
//! | [`client`] | outbound connection wrapper + full-HTTP-message reads |
//! | [`agent_handler`] | encrypt + wrap a client request |
//! | [`server_handler`] | decrypt + dispatch by action; target session map |
//! | [`connection`] | per-connection async driver: SOCKS5, loops, tunnel modes |
//! | [`server`] | accept loop + per-connection spawn |
//! | [`log`] | level-gated file + stdout logging |
//!
//! # Concurrency
//!
//! Pure async/await on a tokio multi-thread runtime (worker count = config
//! `threads`). One task per connection; the C++ Asio callback ladder + strands
//! became linear `async fn`s, with `select!` multiplexing the poll tunnel.

mod agent_handler;
mod client;
mod config;
mod connection;
mod crypto;
mod http;
mod log;
mod server;
mod server_handler;
mod tls;

use std::sync::Arc;

use config::{Config, RunMode};
use log::{Level, Log};

fn main() {
    let args: Vec<String> = std::env::args().collect();

    if args.len() < 3 {
        eprintln!(
            "Usage: {} <mode> <config_file_path>",
            args.first().map(String::as_str).unwrap_or("nipovpn")
        );
        std::process::exit(1);
    }

    let run_mode = match args[1].as_str() {
        "agent" => RunMode::Agent,
        "server" => RunMode::Server,
        other => {
            eprintln!("First argument must be one of [server|agent], got '{other}'");
            std::process::exit(1);
        }
    };

    let config = match Config::load(run_mode, &args[2]) {
        Ok(c) => Arc::new(c),
        Err(e) => {
            eprintln!("Configuration validation failed: {e}");
            std::process::exit(1);
        }
    };

    tls::install_crypto_provider();

    let log = Arc::new(Log::new(&config));

    let threads = config.threads().max(1) as usize;
    let runtime = tokio::runtime::Builder::new_multi_thread()
        .worker_threads(threads)
        .enable_all()
        .build()
        .expect("failed to build tokio runtime");

    if let Err(e) = runtime.block_on(server::run(config.clone(), log.clone())) {
        log.write(&format!("[Runner run] {e}"), Level::Error);
        std::process::exit(1);
    }
}

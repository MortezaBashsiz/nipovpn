//! Server-side request handling. Ports `serverhandler.cpp`.
//!
//! Decrypts the agent's outer request, dispatches on `X-Nipo-Action`:
//! - `send`/`recv`/`close`: poll-tunnel data plane keyed by `X-Nipo-Session`.
//! - `open`/`request` (no action): the inner request — CONNECT (direct tunnel or
//!   poll-mode session) or plain HTTP/HTTPS proxied to the target.

use std::sync::Arc;
use std::time::Duration;

use dashmap::DashMap;
use tokio::sync::Mutex;

use crate::client::Conn;
use crate::config::Config;
use crate::crypto::{aes256_decrypt, aes256_encrypt, decode64, hex_to_bytes};
use crate::http::{extract_body, extract_headers, get_raw_header, HttpType, ParsedHttp};
use crate::log::{Level, Log};

/// Poll-mode target connections, keyed by `X-Nipo-Session`.
pub type Sessions = Arc<DashMap<String, Arc<Mutex<Conn>>>>;

pub struct ServerOutcome {
    pub write_back: Vec<u8>,
    /// True for tunnelEnable CONNECT: caller relays agent <-> `client` directly.
    pub connect: bool,
}

fn conn_value(config: &Config) -> &'static str {
    if config.general.connection_reuse {
        "keep-alive"
    } else {
        "close"
    }
}

fn make_plain_response(body: &[u8], status: &str, connection: &str) -> Vec<u8> {
    let mut out = format!(
        "HTTP/1.1 {status}\r\n\
         Content-Type: application/text\r\n\
         Content-Length: {}\r\n\
         Connection: {connection}\r\n\
         \r\n",
        body.len()
    )
    .into_bytes();
    out.extend_from_slice(body);
    out
}

fn make_encrypted_response(
    config: &Config,
    log: &Log,
    uuid: &str,
    plain: &[u8],
    status: &str,
    connection: &str,
) -> Vec<u8> {
    match aes256_encrypt(plain, config.general.token.as_bytes()) {
        Ok(ct) => make_plain_response(&ct, status, connection),
        Err(e) => {
            log.write(
                &format!("[{uuid}] [ServerHandler] [Encryption Failed] {e}"),
                Level::Debug,
            );
            make_plain_response(b"", "500 Internal Server Error", "close")
        }
    }
}

pub async fn handle(
    config: &Config,
    log: &Log,
    sessions: &Sessions,
    client: &mut Conn,
    uuid: &str,
    src: &str,
    raw: &[u8],
) -> ServerOutcome {
    let plain = |body: &[u8], status: &str, connection: &str| ServerOutcome {
        write_back: make_plain_response(body, status, connection),
        connect: false,
    };
    let enc = |plain: &[u8], status: &str, connection: &str| ServerOutcome {
        write_back: make_encrypted_response(config, log, uuid, plain, status, connection),
        connect: false,
    };

    if ParsedHttp::detect(raw).is_none() {
        log.write(
            &format!(
                "[{uuid}] [ServerHandler handle] [NOT HTTP Request From Agent] [Request] : {}",
                String::from_utf8_lossy(raw)
            ),
            Level::Debug,
        );
        return plain(b"", "400 Bad Request", "close");
    }

    let text = String::from_utf8_lossy(raw);
    let headers = extract_headers(&text);
    let session = get_raw_header(headers, "X-Nipo-Session");
    let action = get_raw_header(headers, "X-Nipo-Action");
    let body = extract_body(&text);

    let decrypted = match decode64(body).and_then(|d| aes256_decrypt(&d, config.general.token.as_bytes())) {
        Ok(d) => d,
        Err(e) => {
            log.write(
                &format!("[{uuid}] [ServerHandler handle] [Decrypt/Decode Failed] {e}"),
                Level::Debug,
            );
            return plain(b"", "400 Bad Request", "close");
        }
    };
    let decrypted_str = String::from_utf8_lossy(&decrypted);

    log.write(
        &format!("[{uuid}] [ServerHandler handle] [Decryption Done]"),
        Level::Debug,
    );

    let cv = conn_value(config);

    // --- poll-tunnel data plane ---
    if action == "send" {
        if let Some(entry) = sessions.get(&session) {
            let target = entry.clone();
            drop(entry);
            let mut t = target.lock().await;
            if t.is_open() {
                let raw_bytes = hex_to_bytes(&decrypted_str);
                let _ = t.write_all(&raw_bytes).await;
            }
        }
        return enc(b"", "200 OK", cv);
    }

    if action == "recv" {
        let mut data = Vec::new();
        if let Some(entry) = sessions.get(&session) {
            let target = entry.clone();
            drop(entry);
            let mut t = target.lock().await;
            if t.is_open() {
                let max = config.general.buffer_size as usize;
                let to = Duration::from_millis(config.general.pull_timeout as u64);
                data = t.read_available(max, to).await.unwrap_or_default();
            }
        }
        return enc(&data, "200 OK", cv);
    }

    if action == "close" {
        if let Some((_, target)) = sessions.remove(&session) {
            target.lock().await.shutdown();
        }
        return enc(b"", "200 OK", cv);
    }

    // --- inner request (open / request) ---
    let inner = hex_to_bytes(&decrypted_str);
    let header_end = match crate::client::find_subslice(&inner, b"\r\n\r\n") {
        Some(p) => p + 4,
        None => {
            log.write(
                &format!("[{uuid}] [ServerHandler handle] [INNER REQUEST MISSING HEADER END]"),
                Level::Debug,
            );
            return enc(b"", "400 Bad Request", "close");
        }
    };
    let inner_headers = &inner[..header_end];
    let pending_tunnel_data = &inner[header_end..];

    let parsed = match ParsedHttp::detect(inner_headers) {
        Some(p) => p,
        None => {
            log.write(
                &format!(
                    "[{uuid}] [ServerHandler handle] [NOT INNER HTTP Request] [Request] : {}",
                    String::from_utf8_lossy(&inner)
                ),
                Level::Debug,
            );
            return enc(b"", "400 Bad Request", "close");
        }
    };

    match parsed.http_type {
        HttpType::Connect => {
            if config.general.tunnel_enable {
                if client.connect_plain(&parsed.dst_ip, parsed.dst_port).await.is_err() {
                    log.write(
                        &format!(
                            "[{uuid}] [CONNECT] [ERROR] [Resolving Host] [SRC {src}] [DST {}:{}]",
                            parsed.dst_ip, parsed.dst_port
                        ),
                        Level::Info,
                    );
                    return enc(b"HTTP/1.1 502 Bad Gateway\r\n\r\n", "502 Bad Gateway", "close");
                }
                log.write(
                    &format!(
                        "[{uuid}] [CONNECT] [TUNNEL] [SRC {src}] [DST {}:{}]",
                        parsed.dst_ip, parsed.dst_port
                    ),
                    Level::Info,
                );
                return ServerOutcome {
                    write_back: make_encrypted_response(
                        config,
                        log,
                        uuid,
                        b"HTTP/1.1 200 Connection Established\r\n\r\n",
                        "200 OK",
                        "keep-alive",
                    ),
                    connect: true,
                };
            }

            // poll-mode: dedicated target connection stored in the session map
            let mut target = Conn::new();
            if target.connect_plain(&parsed.dst_ip, parsed.dst_port).await.is_err() {
                log.write(
                    &format!(
                        "[{uuid}] [CONNECT] [ERROR] [Resolving Host] [SRC {src}] [DST {}:{}]",
                        parsed.dst_ip, parsed.dst_port
                    ),
                    Level::Info,
                );
                return enc(b"HTTP/1.1 502 Bad Gateway\r\n\r\n", "502 Bad Gateway", "close");
            }
            log.write(
                &format!(
                    "[{uuid}] [CONNECT] [SRC {src}] [DST {}:{}]",
                    parsed.dst_ip, parsed.dst_port
                ),
                Level::Info,
            );

            if !pending_tunnel_data.is_empty() {
                let _ = target.write_all(pending_tunnel_data).await;
            }

            let key = if session.is_empty() {
                uuid.to_string()
            } else {
                session.clone()
            };
            sessions.insert(key, Arc::new(Mutex::new(target)));

            enc(
                b"HTTP/1.1 200 Connection Established\r\n\r\n",
                "200 OK",
                "keep-alive",
            )
        }

        HttpType::Http | HttpType::Https => {
            if !client.is_open()
                && client.connect_plain(&parsed.dst_ip, parsed.dst_port).await.is_err()
            {
                return enc(b"", "502 Bad Gateway", "close");
            }

            if client.write_all(&inner).await.is_err() {
                return enc(b"", "502 Bad Gateway", "close");
            }

            // Read the full target HTTP response (headers + Content-Length /
            // chunked body) rather than a single chunk, so multi-segment
            // responses are returned whole.
            let resp = client.read_http_message().await.unwrap_or_default();

            if resp.is_empty() {
                client.shutdown();
                return enc(b"", "200 OK", cv);
            }

            enc(&resp, "200 OK", cv)
        }
    }
}

//! Per-connection driver. Ports `tcpconnection.{hpp,cpp}`.
//!
//! Replaces the Asio callback ladder with linear async tasks: an agent or
//! server read loop, SOCKS5 handshake, direct-tunnel relay
//! (`copy_bidirectional`), and the poll-tunnel `send`/`recv`/`close` loop.

use std::net::Ipv6Addr;
use std::sync::Arc;
use std::time::Duration;

use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::time::Instant;
use tokio_rustls::TlsConnector;
use uuid::Uuid;

use crate::client::{find_subslice, read_http_message_from, BoxStream, Conn};
use crate::config::Config;
use crate::crypto::{aes256_decrypt, aes256_encrypt, bytes_to_hex, encode64};
use crate::http::{host_from_url, sni_from_url};
use crate::log::Log;
use crate::server_handler::Sessions;
use crate::{agent_handler, server_handler};

pub struct Connection {
    config: Arc<Config>,
    log: Arc<Log>,
    connector: Option<Arc<TlsConnector>>,
    sessions: Sessions,
    uuid: String,
    src: String,
}

impl Connection {
    pub fn new(
        config: Arc<Config>,
        log: Arc<Log>,
        connector: Option<Arc<TlsConnector>>,
        sessions: Sessions,
        src: String,
    ) -> Self {
        Connection {
            config,
            log,
            connector,
            sessions,
            uuid: Uuid::new_v4().to_string(),
            src,
        }
    }

    // --- Agent ---

    pub async fn run_agent(self, mut down: BoxStream) {
        let mut server = Conn::new();

        let mut initial: Option<Vec<u8>> = None;
        if self.config.general.protocol == "socks5" {
            match socks5_handshake(&mut down).await {
                Ok(req) => initial = Some(req),
                Err(_) => return,
            }
        }

        loop {
            let raw = match initial.take() {
                Some(b) => b,
                None => match read_http_message_from(&mut down).await {
                    Ok(r) if !r.is_empty() => r,
                    _ => {
                        server.shutdown();
                        return;
                    }
                },
            };

            let outcome = match agent_handler::handle(
                &self.config,
                &self.log,
                &mut server,
                self.connector.as_deref(),
                &self.uuid,
                &self.src,
                &raw,
            )
            .await
            {
                Ok(Some(o)) => o,
                _ => {
                    server.shutdown();
                    return;
                }
            };

            let write_back = if self.config.general.protocol == "socks5" && outcome.connect {
                vec![0x05, 0x00, 0x00, 0x01, 0, 0, 0, 0, 0, 0]
            } else {
                outcome.write_back
            };

            if write_back.is_empty() || down.write_all(&write_back).await.is_err() {
                server.shutdown();
                return;
            }

            if outcome.connect {
                if self.config.general.tunnel_enable {
                    if let Some(mut s) = server.take_stream() {
                        let _ = tokio::io::copy_bidirectional(&mut down, &mut s).await;
                    }
                } else {
                    if !self.config.general.connection_reuse {
                        server.shutdown();
                    }
                    self.poll_tunnel(down, server).await;
                }
                return;
            }
            // non-CONNECT: keep-alive, read the next request
        }
    }

    /// Poll-tunnel data plane (non-direct CONNECT). One task multiplexes
    /// client->`send` and a periodic `recv`, serialized over the single
    /// nipo-server link via `select!` (mirrors the C++ strand + relayMutex).
    async fn poll_tunnel(&self, down: BoxStream, mut server: Conn) {
        let (mut rd, mut wr) = tokio::io::split(down);
        let mut buf = vec![0u8; self.config.general.buffer_size as usize];

        let pull = Duration::from_millis(self.config.general.pull_timeout.max(1) as u64);
        let mut ticker = tokio::time::interval_at(Instant::now() + pull, pull);
        ticker.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);

        loop {
            tokio::select! {
                r = rd.read(&mut buf) => {
                    match r {
                        Ok(0) | Err(_) => break,
                        Ok(n) => {
                            let chunk = buf[..n].to_vec();
                            self.post_tunnel_action(&mut server, "send", &chunk).await;
                        }
                    }
                }
                _ = ticker.tick() => {
                    if let Some(data) = self.post_tunnel_action(&mut server, "recv", b"").await {
                        if !data.is_empty() && wr.write_all(&data).await.is_err() {
                            break;
                        }
                    }
                }
            }
        }

        self.post_tunnel_action(&mut server, "close", b"").await;
        server.shutdown();
    }

    /// One poll-tunnel round-trip. Ports `postTunnelAction`: ensure the link,
    /// encrypt+wrap the body, send, read the reply, decrypt. Retries once on a
    /// reused connection. Returns the decrypted payload (empty allowed).
    async fn post_tunnel_action(
        &self,
        server: &mut Conn,
        action: &str,
        raw_body: &[u8],
    ) -> Option<Vec<u8>> {
        let mut response = self.tunnel_send_once(server, action, raw_body).await;

        if response.is_none()
            && self.config.general.connection_reuse
            && action != "close"
        {
            server.shutdown();
            response = self.tunnel_send_once(server, action, raw_body).await;
        }

        let response = response?;
        let pos = find_subslice(&response, b"\r\n\r\n")? + 4;
        let encrypted_body = &response[pos..];
        if encrypted_body.is_empty() {
            return None;
        }
        aes256_decrypt(encrypted_body, self.config.general.token.as_bytes()).ok()
    }

    async fn tunnel_send_once(
        &self,
        server: &mut Conn,
        action: &str,
        raw_body: &[u8],
    ) -> Option<Vec<u8>> {
        if !server.is_open() && !self.connect_server(server).await {
            return None;
        }

        let enc = aes256_encrypt(
            bytes_to_hex(raw_body).as_bytes(),
            self.config.general.token.as_bytes(),
        )
        .ok()?;
        let encoded = encode64(&enc);
        let keep_alive = self.config.general.connection_reuse && action != "close";

        let req = self.build_outer(action, &encoded, keep_alive);
        if server.write_all(req.as_bytes()).await.is_err() {
            server.shutdown();
            return None;
        }

        let response = match server.read_http_message().await {
            Ok(r) if !r.is_empty() => r,
            _ => {
                server.shutdown();
                return None;
            }
        };

        if !keep_alive {
            server.shutdown();
        }
        Some(response)
    }

    async fn connect_server(&self, server: &mut Conn) -> bool {
        let ip = &self.config.agent.server_ip;
        let port = self.config.agent.server_port;
        if self.config.general.tls_enable {
            let sni = sni_from_url(&self.config.random_fake_url());
            match self.connector.as_deref() {
                Some(c) => server.connect_tls(ip, port, c, &sni).await.is_ok(),
                None => false,
            }
        } else {
            server.connect_plain(ip, port).await.is_ok()
        }
    }

    fn build_outer(&self, action: &str, body: &str, keep_alive: bool) -> String {
        format!(
            "{method} /{endpoint} HTTP/{ver}\r\n\
             Host: {host}\r\n\
             User-Agent: {ua}\r\n\
             Accept: */*\r\n\
             Content-Type: application/text\r\n\
             X-Nipo-Session: {uuid}\r\n\
             X-Nipo-Action: {action}\r\n\
             Content-Length: {clen}\r\n\
             Connection: {conn}\r\n\
             \r\n\
             {body}",
            method = self.config.random_method(),
            endpoint = self.config.random_end_point(),
            ver = self.config.agent.http_version,
            host = host_from_url(&self.config.random_fake_url()),
            ua = self.config.agent.user_agent,
            uuid = self.uuid,
            action = action,
            clen = body.len(),
            conn = if keep_alive { "keep-alive" } else { "close" },
            body = body,
        )
    }

    // --- Server ---

    pub async fn run_server(self, mut down: BoxStream) {
        let mut target = Conn::new();

        loop {
            let raw = match read_http_message_from(&mut down).await {
                Ok(r) if !r.is_empty() => r,
                _ => {
                    target.shutdown();
                    return;
                }
            };

            let outcome = server_handler::handle(
                &self.config,
                &self.log,
                &self.sessions,
                &mut target,
                &self.uuid,
                &self.src,
                &raw,
            )
            .await;

            if outcome.write_back.is_empty() || down.write_all(&outcome.write_back).await.is_err() {
                target.shutdown();
                return;
            }

            if outcome.connect && self.config.general.tunnel_enable {
                if let Some(mut t) = target.take_stream() {
                    let _ = tokio::io::copy_bidirectional(&mut down, &mut t).await;
                }
                return;
            }

            if !self.config.general.connection_reuse {
                target.shutdown();
                return;
            }
        }
    }
}

/// SOCKS5 greeting + CONNECT, synthesized into an HTTP CONNECT request that
/// feeds the normal path. Ports `handleSocks5Handshake`. The SOCKS success reply
/// is written later by `run_agent` once the tunnel is established.
async fn socks5_handshake(down: &mut BoxStream) -> anyhow::Result<Vec<u8>> {
    let mut header = [0u8; 2];
    down.read_exact(&mut header).await?;
    if header[0] != 0x05 {
        anyhow::bail!("invalid SOCKS5 version");
    }

    let mut methods = vec![0u8; header[1] as usize];
    down.read_exact(&mut methods).await?;

    down.write_all(&[0x05, 0x00]).await?; // no-auth

    let mut req = [0u8; 4];
    down.read_exact(&mut req).await?;
    if req[0] != 0x05 || req[1] != 0x01 {
        let _ = down
            .write_all(&[0x05, 0x07, 0x00, 0x01, 0, 0, 0, 0, 0, 0])
            .await;
        anyhow::bail!("unsupported SOCKS5 command");
    }

    let host = match req[3] {
        0x01 => {
            let mut ip = [0u8; 4];
            down.read_exact(&mut ip).await?;
            format!("{}.{}.{}.{}", ip[0], ip[1], ip[2], ip[3])
        }
        0x03 => {
            let mut len = [0u8; 1];
            down.read_exact(&mut len).await?;
            let mut domain = vec![0u8; len[0] as usize];
            down.read_exact(&mut domain).await?;
            String::from_utf8_lossy(&domain).into_owned()
        }
        0x04 => {
            let mut ip = [0u8; 16];
            down.read_exact(&mut ip).await?;
            Ipv6Addr::from(ip).to_string()
        }
        _ => anyhow::bail!("unsupported SOCKS5 address type"),
    };

    let mut port_bytes = [0u8; 2];
    down.read_exact(&mut port_bytes).await?;
    let port = u16::from_be_bytes(port_bytes);

    let target = if host.contains(':') {
        format!("[{host}]:{port}")
    } else {
        format!("{host}:{port}")
    };

    Ok(format!("CONNECT {target} HTTP/1.1\r\nHost: {target}\r\n\r\n").into_bytes())
}

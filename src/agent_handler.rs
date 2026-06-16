//! Agent-side request handling. Ports `agenthandler.cpp`.
//!
//! Takes the raw client request, wraps it (hex → AES → base64) inside a fake
//! outer HTTP POST, sends it to the nipo-server over the persistent `server`
//! connection, then decrypts the server's reply into bytes to return to the
//! client.

use tokio_rustls::TlsConnector;

use crate::client::{find_subslice, Conn};
use crate::config::Config;
use crate::crypto::{aes256_decrypt, aes256_encrypt, bytes_to_hex, encode64};
use crate::http::{host_from_url, sni_from_url, HttpType, ParsedHttp};
use crate::log::{Level, Log};

pub struct AgentOutcome {
    /// Bytes to write back to the downstream client.
    pub write_back: Vec<u8>,
    /// True when this was a CONNECT (tunnel should follow).
    pub connect: bool,
}

/// Process one client request through the agent. Returns `Ok(None)` to signal
/// the connection should be torn down (matches the C++ `socketShutdown` paths).
pub async fn handle(
    config: &Config,
    log: &Log,
    server: &mut Conn,
    connector: Option<&TlsConnector>,
    uuid: &str,
    src: &str,
    raw: &[u8],
) -> anyhow::Result<Option<AgentOutcome>> {
    let parsed = match ParsedHttp::detect(raw) {
        Some(p) => p,
        None => {
            log.write(
                &format!(
                    "[{uuid}] [AgentHandler handle] [NOT HTTP Request] [Request] : {}",
                    String::from_utf8_lossy(raw)
                ),
                Level::Debug,
            );
            return Ok(None);
        }
    };

    if !parsed.target.is_empty() {
        log.write(
            &format!("[{uuid}] [FORWARD] [SRC {src}] [DST {}]", parsed.target),
            Level::Trace,
        );
    }

    // Ensure the link to the nipo-server is up (reused across requests when
    // connectionReuse is on).
    if !server.is_open() {
        let server_ip = &config.agent.server_ip;
        let server_port = config.agent.server_port;
        let connected = if config.general.tls_enable {
            let sni = sni_from_url(&config.random_fake_url());
            let connector = connector
                .ok_or_else(|| anyhow::anyhow!("tls enabled but no connector"))?;
            server.connect_tls(server_ip, server_port, connector, &sni).await
        } else {
            server.connect_plain(server_ip, server_port).await
        };

        if let Err(e) = connected {
            log.write(
                &format!(
                    "[{uuid}] [CONNECT] [ERROR] [To Server] [SRC {src}] [DST {server_ip}:{server_port}] {e}"
                ),
                Level::Info,
            );
            return Ok(None);
        }
        log.write(
            &format!("[{uuid}] [CONNECT] [SRC {src}] [DST {server_ip}:{server_port}]"),
            Level::Info,
        );
    }

    // hex(raw) -> AES -> base64
    let encrypted = match aes256_encrypt(bytes_to_hex(raw).as_bytes(), config.general.token.as_bytes()) {
        Ok(c) => c,
        Err(e) => {
            log.write(
                &format!("[{uuid}] [AgentHandler handle] [Encryption Failed] : [ {e}] "),
                Level::Debug,
            );
            return Ok(None);
        }
    };
    let inner_request = encode64(&encrypted);

    let is_connect = parsed.http_type == HttpType::Connect;
    let action = if is_connect { "open" } else { "request" };
    let direct_tunnel_open = config.general.tunnel_enable && is_connect;
    let keep_alive = config.general.connection_reuse || direct_tunnel_open;

    let outer = format!(
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
        method = config.random_method(),
        endpoint = config.random_end_point(),
        ver = config.agent.http_version,
        host = host_from_url(&config.random_fake_url()),
        ua = config.agent.user_agent,
        action = action,
        clen = inner_request.len(),
        conn = if keep_alive { "keep-alive" } else { "close" },
        body = inner_request,
    );

    if let Err(e) = server.write_all(outer.as_bytes()).await {
        log.write(
            &format!("[{uuid}] [AgentHandler handle] [write failed] {e}"),
            Level::Debug,
        );
        return Ok(None);
    }

    let response = match server.read_http_message().await {
        Ok(r) => r,
        Err(_) => {
            if !config.general.connection_reuse {
                return Ok(None);
            }
            return Ok(None);
        }
    };

    let body_pos = match find_subslice(&response, b"\r\n\r\n") {
        Some(p) => p + 4,
        None => {
            log.write(
                &format!(
                    "[{uuid}] [AgentHandler handle] [NOT HTTP Response] [Response] : {}",
                    String::from_utf8_lossy(&response)
                ),
                Level::Debug,
            );
            return Ok(None);
        }
    };

    let encrypted_body = &response[body_pos..];
    if encrypted_body.is_empty() {
        return Ok(None);
    }

    let decrypted = match aes256_decrypt(encrypted_body, config.general.token.as_bytes()) {
        Ok(d) => d,
        Err(e) => {
            log.write(
                &format!("[{uuid}] [AgentHandler handle] [Decryption Failed] : [ {e}] "),
                Level::Debug,
            );
            return Ok(None);
        }
    };

    log.write(
        &format!("[{uuid}] [AgentHandler handle] [Decryption Done]"),
        Level::Debug,
    );

    Ok(Some(AgentOutcome {
        write_back: decrypted,
        connect: is_connect,
    }))
}

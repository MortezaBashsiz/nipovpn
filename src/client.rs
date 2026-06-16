//! Outbound connection wrapper. Ports `tcpclient.{hpp,cpp}`.
//!
//! One `Conn` is the agent's link to the nipo-server (optionally TLS) or the
//! server's link to a target host (plain TCP). It exposes the read/write
//! primitives the handlers need, including `read_http_message` which reads a
//! full HTTP message (headers + Content-Length / chunked body).

use std::time::Duration;

use tokio::io::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};
use tokio::net::TcpStream;
use tokio_rustls::TlsConnector;

use crate::http::{extract_body, extract_headers, is_chunked, parse_content_length};
use crate::tls;

/// Any byte stream we relay over (plain TCP or a TLS stream).
pub trait AsyncStream: AsyncRead + AsyncWrite + Unpin + Send {}
impl<T: AsyncRead + AsyncWrite + Unpin + Send> AsyncStream for T {}

pub type BoxStream = Box<dyn AsyncStream>;

pub struct Conn {
    stream: Option<BoxStream>,
}

impl Conn {
    pub fn new() -> Self {
        Conn { stream: None }
    }

    pub fn is_open(&self) -> bool {
        self.stream.is_some()
    }

    /// Plain TCP connect.
    pub async fn connect_plain(&mut self, host: &str, port: u16) -> anyhow::Result<()> {
        if self.stream.is_some() {
            return Ok(());
        }
        let stream = TcpStream::connect((host, port)).await?;
        stream.set_nodelay(true).ok();
        self.stream = Some(Box::new(stream));
        Ok(())
    }

    /// TCP connect + TLS client handshake with the given SNI.
    pub async fn connect_tls(
        &mut self,
        host: &str,
        port: u16,
        connector: &TlsConnector,
        sni: &str,
    ) -> anyhow::Result<()> {
        if self.stream.is_some() {
            return Ok(());
        }
        let tcp = TcpStream::connect((host, port)).await?;
        tcp.set_nodelay(true).ok();
        let server_name = tls::server_name(sni)?;
        let tls_stream = connector.connect(server_name, tcp).await?;
        self.stream = Some(Box::new(tls_stream));
        Ok(())
    }

    pub async fn write_all(&mut self, data: &[u8]) -> anyhow::Result<()> {
        let s = self
            .stream
            .as_mut()
            .ok_or_else(|| anyhow::anyhow!("write on closed conn"))?;
        if let Err(e) = s.write_all(data).await {
            self.shutdown();
            return Err(e.into());
        }
        Ok(())
    }

    /// Read one full HTTP message: headers up to `\r\n\r\n`, then the body per
    /// Content-Length or chunked Transfer-Encoding. Ports `readHttpMessageImpl`.
    pub async fn read_http_message(&mut self) -> anyhow::Result<Vec<u8>> {
        let s = self
            .stream
            .as_mut()
            .ok_or_else(|| anyhow::anyhow!("read on closed conn"))?;
        read_http_message_from(s).await
    }

    /// Read whatever is available within `timeout`; empty Vec on timeout/no data.
    /// Used by the server's poll-tunnel `recv` action.
    pub async fn read_available(
        &mut self,
        max: usize,
        timeout: Duration,
    ) -> anyhow::Result<Vec<u8>> {
        let s = self
            .stream
            .as_mut()
            .ok_or_else(|| anyhow::anyhow!("read on closed conn"))?;
        let mut tmp = vec![0u8; max];
        match tokio::time::timeout(timeout, s.read(&mut tmp)).await {
            Ok(Ok(n)) => {
                tmp.truncate(n);
                Ok(tmp)
            }
            Ok(Err(e)) => Err(e.into()),
            Err(_) => Ok(Vec::new()),
        }
    }

    pub fn shutdown(&mut self) {
        self.stream = None;
    }

    /// Take the underlying stream for direct relay (CONNECT tunnel).
    pub fn take_stream(&mut self) -> Option<BoxStream> {
        self.stream.take()
    }
}

impl Default for Conn {
    fn default() -> Self {
        Conn::new()
    }
}

/// Read a full HTTP message from any stream (headers + Content-Length/chunked
/// body). Shared by `Conn::read_http_message` and the connection driver's
/// downstream socket. Ports `readHttpMessageImpl`.
pub async fn read_http_message_from<S: AsyncRead + Unpin>(s: &mut S) -> anyhow::Result<Vec<u8>> {
    let mut buf = Vec::new();
    let mut tmp = [0u8; 8192];

    loop {
        if find_subslice(&buf, b"\r\n\r\n").is_some() {
            break;
        }
        let n = s.read(&mut tmp).await?;
        if n == 0 {
            if buf.is_empty() {
                anyhow::bail!("eof before headers");
            }
            return Ok(buf);
        }
        buf.extend_from_slice(&tmp[..n]);
    }

    let text = String::from_utf8_lossy(&buf).into_owned();
    let headers = extract_headers(&text);

    if let Some(content_length) = parse_content_length(headers) {
        let mut body_len = extract_body(&text).len();
        while body_len < content_length {
            let n = s.read(&mut tmp).await?;
            if n == 0 {
                anyhow::bail!("eof in body");
            }
            buf.extend_from_slice(&tmp[..n]);
            body_len += n;
        }
    } else if is_chunked(headers) {
        while find_subslice(&buf, b"\r\n0\r\n\r\n").is_none()
            && find_subslice(&buf, b"\r\n0\r\n").is_none()
        {
            let n = s.read(&mut tmp).await?;
            if n == 0 {
                anyhow::bail!("eof in chunked body");
            }
            buf.extend_from_slice(&tmp[..n]);
        }
    }

    Ok(buf)
}

pub fn find_subslice(haystack: &[u8], needle: &[u8]) -> Option<usize> {
    if needle.is_empty() || haystack.len() < needle.len() {
        return None;
    }
    haystack
        .windows(needle.len())
        .position(|w| w == needle)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn subslice_finds() {
        assert_eq!(find_subslice(b"abc\r\n\r\nxy", b"\r\n\r\n"), Some(3));
        assert_eq!(find_subslice(b"abc", b"\r\n\r\n"), None);
    }
}

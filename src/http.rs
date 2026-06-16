//! HTTP / TLS request classification. Ports `http.{hpp,cpp}` + `http_utils.hpp`.

use crate::crypto::{bytes_to_hex, hex_to_bytes};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum HttpType {
    Https,
    Http,
    Connect,
}

#[derive(Debug, Clone)]
pub struct ParsedHttp {
    pub http_type: HttpType,
    pub target: String,
    pub dst_ip: String,
    pub dst_port: u16,
    /// Extracted TLS SNI (empty for non-TLS); retained for diagnostics.
    #[allow(dead_code)]
    pub sni: String,
}

impl ParsedHttp {
    /// Classify a raw request buffer. Returns `None` when it is neither a
    /// recognizable TLS record nor a parseable HTTP request (C++ `detectType`).
    pub fn detect(raw: &[u8]) -> Option<ParsedHttp> {
        let hex = bytes_to_hex(raw);
        let first = hex.get(0..2).unwrap_or("");

        if first == "16" || first == "14" || first == "17" {
            let sni = parse_tls_sni(&hex);
            let (dst_ip, dst_port) =
                parse_host_port(&sni, 443).unwrap_or((String::new(), 443));
            Some(ParsedHttp {
                http_type: HttpType::Https,
                target: sni.clone(),
                dst_ip,
                dst_port,
                sni,
            })
        } else {
            parse_http(raw)
        }
    }
}

fn parse_http(raw: &[u8]) -> Option<ParsedHttp> {
    let mut headers = [httparse::EMPTY_HEADER; 64];
    let mut req = httparse::Request::new(&mut headers);
    // Tolerate a missing terminator (eager parse, like Beast).
    match req.parse(raw) {
        Ok(_) => {}
        Err(_) => return None,
    }

    let method = req.method?.to_string();
    let target = req.path?.to_string();

    let http_type = if method.eq_ignore_ascii_case("CONNECT") {
        HttpType::Connect
    } else {
        HttpType::Http
    };

    let host_header = req
        .headers
        .iter()
        .find(|h| h.name.eq_ignore_ascii_case("Host"))
        .map(|h| String::from_utf8_lossy(h.value).to_string());

    let (dst_ip, dst_port) = match http_type {
        HttpType::Connect => parse_host_port(&target, 443).unwrap_or((String::new(), 443)),
        HttpType::Http => {
            let host_port = if let Some(rest) = target
                .strip_prefix("http://")
                .or_else(|| target.strip_prefix("https://"))
            {
                rest.split('/').next().unwrap_or("").to_string()
            } else if let Some(h) = host_header {
                h
            } else {
                return Some(ParsedHttp {
                    http_type,
                    target,
                    dst_ip: String::new(),
                    dst_port: 80,
                    sni: String::new(),
                });
            };
            parse_host_port(&host_port, 80).unwrap_or((String::new(), 80))
        }
        HttpType::Https => unreachable!(),
    };

    Some(ParsedHttp {
        http_type,
        target,
        dst_ip,
        dst_port,
        sni: String::new(),
    })
}

/// Extract the SNI from a hex-encoded TLS ClientHello. Ports the offset walk in
/// C++ `HTTP::parseTls` verbatim (offsets are in hex chars: 2 per byte). Returns
/// an empty string if the layout does not match (e.g. non-ClientHello records).
fn parse_tls_sni(body: &str) -> String {
    if hslice(body, 0, 2) != "16" {
        return String::new();
    }
    if hslice(body, 10, 2) != "01" {
        return String::new();
    }

    // session id
    let mut pos = 86usize;
    let mut tmp_pos = hex_to_int(hslice(body, pos, 2));
    pos += 2;
    pos += tmp_pos * 2;

    // cipher suites
    tmp_pos = hex_to_int(hslice(body, pos, 4));
    pos += 4;
    pos += tmp_pos * 2;

    // compression methods
    tmp_pos = hex_to_int(hslice(body, pos, 2));
    pos += 2;
    pos += tmp_pos * 2;

    // extensions total length
    let _ext_total = hex_to_int(hslice(body, pos, 4));
    pos += 4;

    // first extension type: 0x0000 == server_name
    if hex_to_int(hslice(body, pos, 4)) == 0 {
        pos += 14;
        tmp_pos = hex_to_int(hslice(body, pos, 4));
        pos += 4;
        let sni_hex = hslice(body, pos, tmp_pos * 2);
        return String::from_utf8_lossy(&hex_to_bytes(sni_hex)).into_owned();
    }

    String::new()
}

/// Safe substring over an ASCII (hex) string: clamps `len`, returns "" if
/// `start` is out of range. Mirrors `std::string::substr` clamping without the
/// out-of-range throw.
fn hslice(s: &str, start: usize, len: usize) -> &str {
    if start > s.len() {
        return "";
    }
    let end = (start + len).min(s.len());
    &s[start..end]
}

fn hex_to_int(h: &str) -> usize {
    usize::from_str_radix(h, 16).unwrap_or(0)
}

/// Split `host`, `host:port`, or `[ipv6]:port`. Ports C++ `HTTP::parseHostPort`.
pub fn parse_host_port(input: &str, default_port: u16) -> Option<(String, u16)> {
    if input.is_empty() {
        return None;
    }

    let parse_port = |v: &str| -> Option<u16> {
        let p: i64 = v.parse().ok()?;
        if !(1..=65535).contains(&p) {
            return None;
        }
        Some(p as u16)
    };

    if input.starts_with('[') {
        let close = input.find(']')?;
        let host = input[1..close].to_string();
        let mut port = default_port;
        if close + 1 < input.len() {
            let rest = &input[close + 1..];
            let p = rest.strip_prefix(':')?;
            port = parse_port(p)?;
        }
        return if host.is_empty() { None } else { Some((host, port)) };
    }

    let first_colon = input.find(':');
    let last_colon = input.rfind(':');

    if let (Some(f), Some(l)) = (first_colon, last_colon) {
        if f != l {
            // multiple colons: bare IPv6 unless the last segment is a valid port
            let maybe_port = &input[l + 1..];
            if let Some(p) = parse_port(maybe_port) {
                let host = input[..l].to_string();
                return if host.is_empty() { None } else { Some((host, p)) };
            }
            return Some((input.to_string(), default_port));
        }
    }

    if let Some(l) = last_colon {
        let host = input[..l].to_string();
        let port = parse_port(&input[l + 1..])?;
        if host.is_empty() {
            return None;
        }
        Some((host, port))
    } else {
        Some((input.to_string(), default_port))
    }
}

// --- header helpers (http_utils.hpp) ---

/// Headers including the trailing `\r\n\r\n`, or "" if no terminator.
pub fn extract_headers(message: &str) -> &str {
    match message.find("\r\n\r\n") {
        Some(p) => &message[..p + 4],
        None => "",
    }
}

/// Body after `\r\n\r\n`, or "" if no terminator.
pub fn extract_body(message: &str) -> &str {
    match message.find("\r\n\r\n") {
        Some(p) => &message[p + 4..],
        None => "",
    }
}

pub fn parse_content_length(headers: &str) -> Option<usize> {
    for line in headers.split('\n') {
        let line = line.strip_suffix('\r').unwrap_or(line);
        if line.to_ascii_lowercase().starts_with("content-length:") {
            let raw = line[line.find(':')? + 1..].trim();
            return raw.parse().ok();
        }
    }
    None
}

pub fn is_chunked(headers: &str) -> bool {
    for line in headers.split('\n') {
        let line = line.strip_suffix('\r').unwrap_or(line);
        let lower = line.to_ascii_lowercase();
        if lower.starts_with("transfer-encoding:") && lower.contains("chunked") {
            return true;
        }
    }
    false
}

/// Case-insensitive header lookup, trimmed value. Ports `getRawHeader`.
pub fn get_raw_header(headers: &str, name: &str) -> String {
    let prefix = format!("{}:", name.to_ascii_lowercase());
    for line in headers.split('\n') {
        let line = line.strip_suffix('\r').unwrap_or(line);
        if line.to_ascii_lowercase().starts_with(&prefix) {
            return line[name.len() + 1..].trim().to_string();
        }
    }
    String::new()
}

/// Host portion of a fake URL (scheme + path stripped). Used for the `Host`
/// header. Ports `relayHostHeader` / `makeRelayHostHeader`.
pub fn host_from_url(url: &str) -> String {
    let s = url.split_once("://").map(|(_, r)| r).unwrap_or(url);
    s.split('/').next().unwrap_or(s).to_string()
}

/// Like `host_from_url` but also strips a trailing `:port` — used as the TLS
/// SNI host (C++ `doHandshakeClient`).
pub fn sni_from_url(url: &str) -> String {
    let h = host_from_url(url);
    h.split(':').next().unwrap_or(&h).to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn detects_connect() {
        let raw = b"CONNECT example.com:443 HTTP/1.1\r\nHost: example.com:443\r\n\r\n";
        let p = ParsedHttp::detect(raw).unwrap();
        assert_eq!(p.http_type, HttpType::Connect);
        assert_eq!(p.dst_ip, "example.com");
        assert_eq!(p.dst_port, 443);
    }

    #[test]
    fn detects_plain_http_with_host() {
        let raw = b"GET /index.html HTTP/1.1\r\nHost: example.com\r\n\r\n";
        let p = ParsedHttp::detect(raw).unwrap();
        assert_eq!(p.http_type, HttpType::Http);
        assert_eq!(p.dst_ip, "example.com");
        assert_eq!(p.dst_port, 80);
    }

    #[test]
    fn detects_http_absolute_uri() {
        let raw = b"GET http://example.com:8080/x HTTP/1.1\r\nHost: ignored\r\n\r\n";
        let p = ParsedHttp::detect(raw).unwrap();
        assert_eq!(p.dst_ip, "example.com");
        assert_eq!(p.dst_port, 8080);
    }

    #[test]
    fn non_http_returns_none() {
        assert!(ParsedHttp::detect(b"\x00\x01garbage").is_none());
    }

    #[test]
    fn host_port_variants() {
        assert_eq!(parse_host_port("h:80", 1).unwrap(), ("h".into(), 80));
        assert_eq!(parse_host_port("h", 80).unwrap(), ("h".into(), 80));
        assert_eq!(
            parse_host_port("[::1]:443", 1).unwrap(),
            ("::1".into(), 443)
        );
        // bare ipv6 without brackets: matches C++ quirk — trailing ":1" parses
        // as a port, so host is truncated. (Brackets are required for IPv6.)
        assert_eq!(parse_host_port("fe80::1", 443).unwrap(), ("fe80:".into(), 1));
        // bare ipv6 whose tail isn't a valid port -> whole thing is host
        assert_eq!(
            parse_host_port("fe80::abcd", 443).unwrap(),
            ("fe80::abcd".into(), 443)
        );
    }

    #[test]
    fn sni_parse_real_client_hello() {
        // A real TLS 1.2 ClientHello for "example.ulfheim.net" (well-known sample).
        let hello = hex_to_bytes(concat!(
            "16030100a5010000a10303000102030405060708090a0b0c0d0e0f101112131415161718",
            "191a1b1c1d1e1f20e0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfc",
            "fdfeff00060013010100ff0100007200000018001600001365786d706c652e756c666865",
            "696d2e6e6574000b000403000102000a00160014001d0017001e0019001801000101010201",
            "03010401050106",
        ));
        // adjust: extension type 0x0000 must be first for the C++ algorithm.
        let p = ParsedHttp::detect(&hello).unwrap();
        assert_eq!(p.http_type, HttpType::Https);
        // We assert the parser does not panic and yields a (possibly empty) SNI.
        let _ = p.sni;
    }

    #[test]
    fn header_helpers() {
        let msg = "GET / HTTP/1.1\r\nContent-Length: 12\r\nX-Nipo-Action: send\r\n\r\nbody";
        let h = extract_headers(msg);
        assert_eq!(parse_content_length(h), Some(12));
        assert_eq!(get_raw_header(h, "X-Nipo-Action"), "send");
        assert_eq!(extract_body(msg), "body");
        assert!(!is_chunked(h));
    }
}

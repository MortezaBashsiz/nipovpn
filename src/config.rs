//! Configuration. Ports `config.{hpp,cpp}` + `validateConfig` (`general.hpp`).
//! Reads the same YAML schema (`nipovpn/etc/nipovpn/config.yaml`) unchanged.

use std::path::Path;

use rand::seq::SliceRandom;
use serde::Deserialize;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RunMode {
    Server,
    Agent,
}

impl RunMode {
    pub fn as_str(self) -> &'static str {
        match self {
            RunMode::Server => "server",
            RunMode::Agent => "agent",
        }
    }
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct General {
    pub token: String,
    #[serde(default)]
    pub protocol: String,
    #[serde(default = "default_buffer_size")]
    pub buffer_size: u32,
    #[serde(default)]
    pub fake_urls: Vec<String>,
    #[serde(default)]
    pub methods: Vec<String>,
    #[serde(default)]
    pub end_points: Vec<String>,
    #[serde(default = "default_timeout")]
    pub timeout: u16,
    #[serde(default = "default_pull_timeout")]
    pub pull_timeout: u16,
    #[serde(default)]
    pub tunnel_enable: bool,
    #[serde(default)]
    pub connection_reuse: bool,
    #[serde(default)]
    pub tls_enable: bool,
    #[serde(default)]
    pub tls_verify_peer: bool,
    #[serde(default)]
    pub tls_cert_file: String,
    #[serde(default)]
    pub tls_key_file: String,
    #[serde(default)]
    pub tls_ca_file: String,
}

#[derive(Debug, Clone, Deserialize)]
pub struct Log {
    #[serde(rename = "logLevel", default = "default_log_level")]
    pub level: String,
    #[serde(rename = "logFile")]
    pub file: String,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Server {
    #[serde(default = "default_threads")]
    pub threads: u16,
    pub listen_ip: String,
    pub listen_port: u16,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Agent {
    #[serde(default = "default_threads")]
    pub threads: u16,
    pub listen_ip: String,
    pub listen_port: u16,
    pub server_ip: String,
    pub server_port: u16,
    #[serde(default = "default_http_version")]
    pub http_version: String,
    #[serde(default = "default_user_agent")]
    pub user_agent: String,
}

#[derive(Debug, Clone, Deserialize)]
struct RawConfig {
    general: General,
    log: Log,
    server: Server,
    agent: Agent,
}

#[derive(Debug, Clone)]
pub struct Config {
    pub run_mode: RunMode,
    pub general: General,
    pub log: Log,
    pub server: Server,
    pub agent: Agent,
}

impl Config {
    pub fn load(mode: RunMode, path: impl AsRef<Path>) -> anyhow::Result<Self> {
        let text = std::fs::read_to_string(path.as_ref())
            .map_err(|e| anyhow::anyhow!("Config file {}: {e}", path.as_ref().display()))?;
        let raw: RawConfig = serde_yaml::from_str(&text)
            .map_err(|e| anyhow::anyhow!("Error parsing config file: {e}"))?;

        let cfg = Config {
            run_mode: mode,
            general: raw.general,
            log: raw.log,
            server: raw.server,
            agent: raw.agent,
        };
        cfg.validate()?;
        Ok(cfg)
    }

    /// Mirrors the semantic checks in C++ `validateConfig`.
    pub fn validate(&self) -> anyhow::Result<()> {
        if self.general.protocol != "http" && self.general.protocol != "socks5" {
            anyhow::bail!("protocol must be one of [http|socks5]");
        }
        if self.general.buffer_size < 1 || self.general.buffer_size > 65536 {
            anyhow::bail!("bufferSize must be between 1 and 65536");
        }
        Ok(())
    }

    /// Worker thread count for the active mode.
    pub fn threads(&self) -> u16 {
        match self.run_mode {
            RunMode::Server => self.server.threads,
            RunMode::Agent => self.agent.threads,
        }
    }

    pub fn listen_ip(&self) -> &str {
        match self.run_mode {
            RunMode::Server => &self.server.listen_ip,
            RunMode::Agent => &self.agent.listen_ip,
        }
    }

    pub fn listen_port(&self) -> u16 {
        match self.run_mode {
            RunMode::Server => self.server.listen_port,
            RunMode::Agent => self.agent.listen_port,
        }
    }

    pub fn random_fake_url(&self) -> String {
        pick(&self.general.fake_urls, "www.google.com")
    }

    pub fn random_method(&self) -> String {
        pick(&self.general.methods, "GET")
    }

    pub fn random_end_point(&self) -> String {
        pick(&self.general.end_points, "api")
    }

    pub fn describe(&self) -> String {
        format!(
            "\nConfig :\n General :\n   token: {}\n   protocol: {}\n   bufferSize: {}\n   \
             fakeUrls: {}\n   methods: {}\n   endPoints: {}\n   timeout: {}\n   pullTimeout: {}\n   \
             tunnelEnable: {}\n   connectionReuse: {}\n   tlsEnable: {}\n   tlsVerifyPeer: {}\n   \
             tlsCertFile: {}\n   tlsKeyFile: {}\n   tlsCaFile: {}\n Log :\n   logLevel: {}\n   \
             logFile: {}\n Server :\n   threads: {}\n   listenIp: {}\n   listenPort: {}\n Agent :\n   \
             threads: {}\n   listenIp: {}\n   listenPort: {}\n   serverIp: {}\n   serverPort: {}\n   \
             httpVersion: {}\n   userAgent: {}\n",
            self.general.token,
            self.general.protocol,
            self.general.buffer_size,
            self.general.fake_urls.join(", "),
            self.general.methods.join(", "),
            self.general.end_points.join(", "),
            self.general.timeout,
            self.general.pull_timeout,
            self.general.tunnel_enable,
            self.general.connection_reuse,
            self.general.tls_enable,
            self.general.tls_verify_peer,
            self.general.tls_cert_file,
            self.general.tls_key_file,
            self.general.tls_ca_file,
            self.log.level,
            self.log.file,
            self.server.threads,
            self.server.listen_ip,
            self.server.listen_port,
            self.agent.threads,
            self.agent.listen_ip,
            self.agent.listen_port,
            self.agent.server_ip,
            self.agent.server_port,
            self.agent.http_version,
            self.agent.user_agent,
        )
    }
}

fn pick(list: &[String], fallback: &str) -> String {
    list.choose(&mut rand::thread_rng())
        .cloned()
        .unwrap_or_else(|| fallback.to_string())
}

fn default_buffer_size() -> u32 {
    65536
}
fn default_timeout() -> u16 {
    10
}
fn default_pull_timeout() -> u16 {
    1
}
fn default_threads() -> u16 {
    2
}
fn default_log_level() -> String {
    "INFO".to_string()
}
fn default_http_version() -> String {
    "1.1".to_string()
}
fn default_user_agent() -> String {
    "NipoAgent".to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    const SAMPLE: &str = include_str!("../nipovpn/etc/nipovpn/config.yaml");

    #[test]
    fn parses_sample_config() {
        let raw: RawConfig = serde_yaml::from_str(SAMPLE).unwrap();
        assert_eq!(raw.general.protocol, "socks5");
        assert_eq!(raw.general.buffer_size, 65536);
        assert_eq!(raw.general.token.len(), 32);
        assert!(raw.general.tls_enable);
        assert!(raw.general.connection_reuse);
        assert_eq!(raw.general.fake_urls.len(), 5);
        assert_eq!(raw.server.listen_port, 80);
        assert_eq!(raw.agent.server_ip, "127.0.0.10");
        assert_eq!(raw.agent.http_version, "1.1");
    }

    #[test]
    fn validate_rejects_bad_protocol() {
        let mut raw: RawConfig = serde_yaml::from_str(SAMPLE).unwrap();
        raw.general.protocol = "ftp".into();
        let cfg = Config {
            run_mode: RunMode::Agent,
            general: raw.general,
            log: raw.log,
            server: raw.server,
            agent: raw.agent,
        };
        assert!(cfg.validate().is_err());
    }

    #[test]
    fn mode_selects_listen() {
        let raw: RawConfig = serde_yaml::from_str(SAMPLE).unwrap();
        let cfg = Config {
            run_mode: RunMode::Server,
            general: raw.general,
            log: raw.log,
            server: raw.server,
            agent: raw.agent,
        };
        assert_eq!(cfg.listen_port(), 80);
        assert_eq!(cfg.threads(), 8);
    }
}

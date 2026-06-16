//! rustls server/client config. Replaces the OpenSSL context setup in
//! `tcpconnection.cpp` (server) and `tcpclient.cpp` (client).
//!
//! Server: load cert chain + private key, no client auth, ALPN http/1.1.
//! Client: verify peer against an optional CA, or accept any cert when
//! `tlsVerifyPeer` is false (matches C++ `verify_none`). ALPN http/1.1.

use std::sync::Arc;

use rustls::client::danger::{HandshakeSignatureValid, ServerCertVerified, ServerCertVerifier};
use rustls::crypto::{verify_tls12_signature, verify_tls13_signature, CryptoProvider};
use rustls::pki_types::{CertificateDer, PrivateKeyDer, ServerName, UnixTime};
use rustls::{ClientConfig, DigitallySignedStruct, RootCertStore, ServerConfig, SignatureScheme};
use tokio_rustls::{TlsAcceptor, TlsConnector};

use crate::config::Config;

const ALPN_HTTP11: &[u8] = b"http/1.1";

/// Install the process-wide ring crypto provider. Call once at startup.
pub fn install_crypto_provider() {
    let _ = rustls::crypto::ring::default_provider().install_default();
}

pub fn build_server_acceptor(config: &Config) -> anyhow::Result<TlsAcceptor> {
    let certs = load_certs(&config.general.tls_cert_file)?;
    let key = load_key(&config.general.tls_key_file)?;

    let mut cfg = ServerConfig::builder()
        .with_no_client_auth()
        .with_single_cert(certs, key)
        .map_err(|e| anyhow::anyhow!("TLS server cert/key error: {e}"))?;
    cfg.alpn_protocols = vec![ALPN_HTTP11.to_vec()];

    Ok(TlsAcceptor::from(Arc::new(cfg)))
}

pub fn build_client_connector(config: &Config) -> anyhow::Result<TlsConnector> {
    let mut cfg = if config.general.tls_verify_peer {
        let mut roots = RootCertStore::empty();
        if !config.general.tls_ca_file.is_empty() {
            for cert in load_certs(&config.general.tls_ca_file)? {
                roots.add(cert)?;
            }
        } else {
            roots.extend(webpki_roots_or_empty());
        }
        ClientConfig::builder()
            .with_root_certificates(roots)
            .with_no_client_auth()
    } else {
        ClientConfig::builder()
            .dangerous()
            .with_custom_certificate_verifier(Arc::new(NoCertVerify::new()))
            .with_no_client_auth()
    };
    cfg.alpn_protocols = vec![ALPN_HTTP11.to_vec()];

    Ok(TlsConnector::from(Arc::new(cfg)))
}

/// Build a rustls `ServerName` for SNI from a host string.
pub fn server_name(host: &str) -> anyhow::Result<ServerName<'static>> {
    ServerName::try_from(host.to_string())
        .map_err(|_| anyhow::anyhow!("invalid SNI host: {host}"))
}

fn load_certs(path: &str) -> anyhow::Result<Vec<CertificateDer<'static>>> {
    let data = std::fs::read(path)
        .map_err(|e| anyhow::anyhow!("reading cert file {path}: {e}"))?;
    let certs: Vec<_> = rustls_pemfile::certs(&mut data.as_slice())
        .collect::<Result<_, _>>()
        .map_err(|e| anyhow::anyhow!("parsing certs in {path}: {e}"))?;
    if certs.is_empty() {
        anyhow::bail!("no certificates found in {path}");
    }
    Ok(certs)
}

fn load_key(path: &str) -> anyhow::Result<PrivateKeyDer<'static>> {
    let data = std::fs::read(path)
        .map_err(|e| anyhow::anyhow!("reading key file {path}: {e}"))?;
    rustls_pemfile::private_key(&mut data.as_slice())
        .map_err(|e| anyhow::anyhow!("parsing key in {path}: {e}"))?
        .ok_or_else(|| anyhow::anyhow!("no private key found in {path}"))
}

/// No webpki-roots dependency; verify-peer with the system trust store is out of
/// scope (the deployment supplies a CA file). Returns empty when none given.
fn webpki_roots_or_empty() -> Vec<rustls::pki_types::TrustAnchor<'static>> {
    Vec::new()
}

/// Accept any server certificate. Mirrors OpenSSL `verify_none` used when
/// `tlsVerifyPeer` is false.
#[derive(Debug)]
struct NoCertVerify {
    provider: Arc<CryptoProvider>,
}

impl NoCertVerify {
    fn new() -> Self {
        NoCertVerify {
            provider: rustls::crypto::ring::default_provider().into(),
        }
    }
}

impl ServerCertVerifier for NoCertVerify {
    fn verify_server_cert(
        &self,
        _end_entity: &CertificateDer<'_>,
        _intermediates: &[CertificateDer<'_>],
        _server_name: &ServerName<'_>,
        _ocsp_response: &[u8],
        _now: UnixTime,
    ) -> Result<ServerCertVerified, rustls::Error> {
        Ok(ServerCertVerified::assertion())
    }

    fn verify_tls12_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        verify_tls12_signature(
            message,
            cert,
            dss,
            &self.provider.signature_verification_algorithms,
        )
    }

    fn verify_tls13_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        verify_tls13_signature(
            message,
            cert,
            dss,
            &self.provider.signature_verification_algorithms,
        )
    }

    fn supported_verify_schemes(&self) -> Vec<SignatureScheme> {
        self.provider
            .signature_verification_algorithms
            .supported_schemes()
    }
}

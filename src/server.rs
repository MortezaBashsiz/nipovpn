//! Accept loop + per-connection spawn. Ports `tcpserver.{hpp,cpp}` and the
//! io_context/thread-pool role of `runner.{hpp,cpp}` (the thread pool is now the
//! tokio multi-thread runtime created in `main`).

use std::sync::Arc;

use dashmap::DashMap;
use tokio::net::TcpListener;
use tokio_rustls::TlsAcceptor;

use crate::config::{Config, RunMode};
use crate::connection::Connection;
use crate::log::{Level, Log};
use crate::tls;

pub async fn run(config: Arc<Config>, log: Arc<Log>) -> anyhow::Result<()> {
    log.write(
        &format!("Config initialized in {} mode", config.run_mode.as_str()),
        Level::Info,
    );
    log.write(&config.describe(), Level::Info);

    let addr = format!("{}:{}", config.listen_ip(), config.listen_port());
    let listener = TcpListener::bind(&addr).await?;
    log.write(&format!("Listening on {addr}"), Level::Info);

    let connector = if config.run_mode == RunMode::Agent && config.general.tls_enable {
        Some(Arc::new(tls::build_client_connector(&config)?))
    } else {
        None
    };

    let acceptor: Option<TlsAcceptor> =
        if config.run_mode == RunMode::Server && config.general.tls_enable {
            Some(tls::build_server_acceptor(&config)?)
        } else {
            None
        };

    let sessions = Arc::new(DashMap::new());

    loop {
        let (stream, peer) = match listener.accept().await {
            Ok(p) => p,
            Err(e) => {
                log.write(&format!("[TCPServer handleAccept] Error: {e}"), Level::Error);
                continue;
            }
        };
        stream.set_nodelay(true).ok();

        let config = config.clone();
        let log = log.clone();
        let connector = connector.clone();
        let acceptor = acceptor.clone();
        let sessions = sessions.clone();
        let src = peer.to_string();

        tokio::spawn(async move {
            let conn = Connection::new(config.clone(), log.clone(), connector, sessions, src);
            match config.run_mode {
                RunMode::Server => match acceptor {
                    Some(acc) => match acc.accept(stream).await {
                        Ok(tls_stream) => conn.run_server(Box::new(tls_stream)).await,
                        Err(e) => log.write(
                            &format!("[TCPServer handleAccept] TLS handshake failed: {e}"),
                            Level::Error,
                        ),
                    },
                    None => conn.run_server(Box::new(stream)).await,
                },
                RunMode::Agent => conn.run_agent(Box::new(stream)).await,
            }
        });
    }
}

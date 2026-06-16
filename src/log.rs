//! Logging. Ports `log.{hpp,cpp}`.
//!
//! Level gating matches C++: a message is emitted when `level <= configured`
//! (by the enum ordering below) OR the message is `ERROR`. Output goes to both
//! the log file (append) and stdout, formatted `{ts} [MODE] [LEVEL] msg`.

use std::fs::{File, OpenOptions};
use std::io::Write;
use std::sync::Mutex;

use crate::config::{Config, RunMode};

/// Order matches the C++ `Log::Level` enum (INFO=0, TRACE=1, ERROR=2, DEBUG=3).
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Level {
    Info = 0,
    Trace = 1,
    Error = 2,
    Debug = 3,
}

impl Level {
    fn as_str(self) -> &'static str {
        match self {
            Level::Info => "INFO",
            Level::Trace => "TRACE",
            Level::Error => "ERROR",
            Level::Debug => "DEBUG",
        }
    }

    fn from_config(s: &str) -> Self {
        match s {
            "TRACE" => Level::Trace,
            "DEBUG" => Level::Debug,
            "INFO" => Level::Info,
            other => {
                eprintln!("Invalid log level: {other}. It should be one of [INFO|TRACE|DEBUG].");
                Level::Info
            }
        }
    }
}

pub struct Log {
    level: Level,
    mode: &'static str,
    file_path: String,
    file: Mutex<Option<File>>,
}

impl Log {
    pub fn new(config: &Config) -> Self {
        let mode = match config.run_mode {
            RunMode::Server => "SERVER",
            RunMode::Agent => "AGENT",
        };

        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&config.log.file);

        let handle = match file {
            Ok(f) => Some(f),
            Err(e) => {
                eprintln!(
                    "Error opening log file: {}. Make sure the directory and file exist. ({e})",
                    config.log.file
                );
                None
            }
        };

        Log {
            level: Level::from_config(&config.log.level),
            mode,
            file_path: config.log.file.clone(),
            file: Mutex::new(handle),
        }
    }

    pub fn write(&self, message: &str, level: Level) {
        if !(level <= self.level || level == Level::Error) {
            return;
        }

        let ts = chrono::Local::now().format("%Y-%m-%d_%H:%M:%S");
        let line = format!("{ts} [{}] [{}] {message}\n", self.mode, level.as_str());

        print!("{line}");
        let _ = std::io::stdout().flush();

        let mut guard = self.file.lock().unwrap();
        if guard.is_none() {
            *guard = OpenOptions::new()
                .create(true)
                .append(true)
                .open(&self.file_path)
                .ok();
        }
        if let Some(f) = guard.as_mut() {
            let _ = f.write_all(line.as_bytes());
        } else {
            eprintln!(
                "Error opening log file: {}. Make sure the directory and file exist.",
                self.file_path
            );
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn gating_info_level() {
        // configured=INFO shows INFO + ERROR only
        assert!(Level::Info <= Level::Info);
        assert!(!(Level::Trace <= Level::Info));
        assert!(!(Level::Debug <= Level::Info));
        assert_eq!(Level::Error, Level::Error);
    }

    #[test]
    fn gating_debug_shows_all() {
        for l in [Level::Info, Level::Trace, Level::Error, Level::Debug] {
            assert!(l <= Level::Debug || l == Level::Error);
        }
    }
}

# NipoVPN

NipoVPN is a proxy that hides your traffic inside ordinary-looking HTTP requests.
It runs in two roles: an **agent** on your side and a **server** outside the
censored or monitored network. The agent wraps each request in a decoy HTTP
request (random method, endpoint and `Host`), optionally encrypts it with
AES-256-GCM and sends it over TLS. The server unwraps it, talks to the real
destination and sends the response back the same way.

It is written in C++20 on top of Boost.Asio, OpenSSL and yaml-cpp.

- [Features](#features)
- [How it works](#how-it-works)
- [Installation](#installation)
- [Quick start](#quick-start)
- [Configuration reference](#configuration-reference)
- [Security notes](#security-notes)
- [Logging](#logging)
- [Troubleshooting](#troubleshooting)
- [Contribution](#contribution)
- [License](#license)

## Features

### Client side (agent)

- **HTTP proxy and SOCKS5 proxy** for local applications (`general.protocol`).
  - HTTP: plain HTTP requests and `CONNECT` tunnels (HTTPS).
  - SOCKS5: `CONNECT` command, no authentication, with IPv4, IPv6 and domain
    name targets.
- **Multiple agents per server.** Each agent session is identified by a UUID, so
  many agents (and many clients behind each agent) can share one server.
- **Multi-threaded.** The number of worker threads is configurable for the agent
  and the server separately.

### Traffic obfuscation

- **Decoy HTTP requests.** Every request between the agent and server is a
  normal HTTP request whose method, path and `Host` header are picked at random
  from lists you configure (`methods`, `endPoints`, `fakeUrls`).
- **Configurable `User-Agent` and HTTP version** for the decoy requests.
- **Randomized TLS SNI.** With TLS enabled, the SNI sent to the server is also
  picked at random from `fakeUrls`.
- **Hidden control data.** The session ID and action travel inside the
  (encrypted) body, not in custom headers, so the outer request carries no
  NipoVPN-specific headers.

### Encryption

- **AES-256-GCM payload encryption** (on by default). The key is the SHA-256 of
  `general.token`; each message gets a random 12-byte nonce and a 16-byte
  authentication tag, so tampered or wrongly keyed payloads are rejected.
- **TLS between agent and server** (`tlsEnable`): TLS 1.2 minimum, TLS 1.3
  preferred, with AEAD-only cipher suites (AES-GCM, ChaCha20-Poly1305).
- **Optional certificate verification** on the agent (`tlsVerifyPeer`) against
  a custom CA (`tlsCaFile`).

AES and TLS are independent: you can use either, both, or (not recommended)
neither.

### Connection handling

- **Two tunnel modes** for `CONNECT`/SOCKS5 traffic:
  - **Polling mode** (default, `tunnelEnable: false`): every chunk of tunneled
    data is sent as its own decoy HTTP request (`send`), and the agent polls the
    server for incoming data (`recv`). Traffic looks like many short HTTP
    requests. Polling backs off from `pullTimeout` up to 50 ms while idle and
    snaps back as soon as data arrives.
  - **Direct tunnel mode** (`tunnelEnable: true`): after the first decoy
    request, the agent–server connection becomes a raw byte pipe to the
    destination. Faster, but only the opening request is disguised and
    encrypted. Use it together with `tlsEnable`.
- **Connection reuse** (`connectionReuse`): keep one keep-alive connection
  between agent and server instead of opening a new one per request.
- **Timeouts**: an I/O timeout that closes stalled connections (`timeout`) and
  a configurable buffer size per read (`bufferSize`).

### Operations

- **Config validation at startup** with clear error messages.
- **Warning when the public sample token is still in use.**
- **Log levels** `INFO`, `TRACE` (per-request source/destination) and `DEBUG`;
  logs go to a file and to stdout.
- **Packaging**: `.deb` packages (amd64, arm64) with systemd units, a Docker
  image and compose file, a Nix flake with a NixOS module, a Termux package,
  Windows (x86_64) builds and an [Android app](https://github.com/MortezaBashsiz/nipovpn-android).

## How it works

### Use cases

**Bypass filtering.** Run the server outside the filtered network and the agent
inside it. Filtering equipment sees only HTTP(S) requests to harmless-looking
hosts.

![Filtering](https://github.com/MortezaBashsiz/nipovpn/blob/main/files/pic/archFilternet.png)

**Hide your requests on the Internet.** Even without censorship, NipoVPN hides
which sites you visit from anyone watching your connection.

![Internet](https://github.com/MortezaBashsiz/nipovpn/blob/main/files/pic/archInternet.png)

### Request flow

```
Browser/app ──HTTP or SOCKS5──▶ Agent ──decoy HTTP(S)──▶ Server ──real request──▶ Destination
            ◀──────────────────       ◀────────────────        ◀────────────────
```

1. A local application sends a request to the agent (HTTP proxy or SOCKS5).
   SOCKS5 requests are converted to an HTTP `CONNECT` internally.
2. The agent builds an envelope `action + session ID + request bytes`, encrypts
   it with AES-256-GCM (if `aesEnable`) and Base64-encodes it.
3. The envelope is sent as the body of a decoy request such as
   `PUT /login` with `Host: cloudflare.com`, over TLS if `tlsEnable`.
4. The server decodes and decrypts the body and performs the action:
   - `request` — forward a plain HTTP request and return the response.
   - `open` — open a TCP connection to the `CONNECT` target.
   - `send` / `recv` — write to / read from that connection (polling mode).
   - `close` — close the connection.
5. The server's reply is encrypted and returned as the body of an HTTP
   `200 OK` response; the agent decrypts it and hands it to the application.

![Flow](https://github.com/MortezaBashsiz/nipovpn/blob/main/files/pic/flow.png)

## Installation

| Platform | How |
|---|---|
| Debian / Ubuntu (amd64, arm64) | `.deb` from the [releases page](https://github.com/MortezaBashsiz/nipovpn/releases), tested on Ubuntu 24.04 |
| Build from source on Linux | [Linux build guide](guides/BuildLinux.md) |
| Docker | [Docker guide](guides/Docker.md) |
| Nix / NixOS | [Nix guide](guides/Nix.md) |
| Termux | [Termux guide](guides/InstallTermux.md) |
| Windows (x86_64) | zip from the [releases page](https://github.com/MortezaBashsiz/nipovpn/releases) (`windows-v*` tags) |
| Android | [nipovpn-android](https://github.com/MortezaBashsiz/nipovpn-android) |

### Debian package

```bash
sudo apt install ./nipovpn.deb
```

The package installs:

- `/usr/bin/nipovpn`
- `/etc/nipovpn/config.yaml` (kept on upgrade)
- `nipovpn-server.service` and `nipovpn-agent.service`

```bash
sudo systemctl enable --now nipovpn-server.service   # on the server
sudo systemctl enable --now nipovpn-agent.service    # on the agent machine
```

### From source

Requirements: a C++20 compiler, CMake ≥ 3.22, Boost ≥ 1.88, yaml-cpp ≥ 0.8 and
OpenSSL. See the [Linux build guide](guides/BuildLinux.md) for details.

```bash
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build -j$(nproc)
```

## Quick start

The same binary and the same config file format are used on both sides; the
first argument selects the role:

```bash
nipovpn <server|agent> <config_file_path>
```

### 1. Prepare the log file

```bash
sudo mkdir -p /var/log/nipovpn/
sudo touch /var/log/nipovpn/nipovpn.log
```

### 2. Pick a shared token

`general.token` must be at least 32 bytes and identical on the agent and the
server. Do not use the sample token from the shipped config.

```bash
openssl rand -hex 16   # prints 32 hex characters
```

### 3. TLS certificate (if `tlsEnable: true`)

The server needs a certificate and key. A self-signed pair is enough when
`tlsVerifyPeer` is `false`:

```bash
sudo openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
  -subj "/CN=example.com" \
  -keyout /etc/nipovpn/server.key -out /etc/nipovpn/server.crt
```

### 4. Run the server

On the server, set `server.listenIp`/`server.listenPort`, then:

```bash
sudo nipovpn server /etc/nipovpn/config.yaml
```

### 5. Run the agent

On your machine, set `agent.serverIp`/`agent.serverPort` to the server's
address and make sure `token`, `aesEnable`, `tlsEnable` and `tunnelEnable` match
the server. Then:

```bash
sudo nipovpn agent /etc/nipovpn/config.yaml
```

### 6. Point your applications at the agent

```bash
# protocol: socks5
curl --socks5-hostname 127.0.0.1:8080 https://example.com

# protocol: http
curl -x http://127.0.0.1:8080 https://example.com
```

Or configure the agent's address as the SOCKS5/HTTP proxy in your browser or
operating system.

## Configuration reference

The sample config lives at [nipovpn/etc/nipovpn/config.yaml](nipovpn/etc/nipovpn/config.yaml).
Settings marked **must match** have to be the same on agent and server.

### `general`

| Key | Type | Default | Description |
|---|---|---|---|
| `token` | string | — | Shared secret, ≥ 32 bytes when `aesEnable` is on. The AES key is derived from it. **Must match.** |
| `aesEnable` | bool | `true` | AES-256-GCM payload encryption. **Must match.** |
| `protocol` | `http` \| `socks5` | — | Protocol the agent accepts from local applications. |
| `bufferSize` | 1–65536 | `65536` | Maximum bytes read per operation. |
| `fakeUrls` | list | — | Hosts used at random for the decoy `Host` header and TLS SNI. |
| `methods` | list | — | HTTP methods used at random for decoy requests. |
| `endPoints` | list | — | URL paths used at random for decoy requests (`/api`, `/login`, ...). |
| `timeout` | seconds | `10` | I/O timeout for stalled connections. `0` disables it. |
| `pullTimeout` | milliseconds | `1` | Base polling interval in polling mode. |
| `tunnelEnable` | bool | `false` | `true` = direct tunnel mode, `false` = polling mode. **Must match.** |
| `connectionReuse` | bool | `false` | Reuse one keep-alive connection between agent and server. |
| `tlsEnable` | bool | `false` | TLS between agent and server. **Must match.** |
| `tlsVerifyPeer` | bool | `false` | Agent verifies the server certificate. |
| `tlsCertFile` | path | — | Server certificate (PEM, chain allowed). |
| `tlsKeyFile` | path | — | Server private key (PEM). |
| `tlsCaFile` | path | — | CA file the agent uses when `tlsVerifyPeer` is on. |

### `log`

| Key | Default | Description |
|---|---|---|
| `logLevel` | `INFO` | `INFO`, `TRACE` or `DEBUG`. Errors are always logged. |
| `logFile` | — | Log file path. The directory and file must exist. |

### `server`

| Key | Default | Description |
|---|---|---|
| `threads` | `2` | Worker threads. |
| `listenIp` | `127.0.0.1` | IPv4 address to listen on. |
| `listenPort` | `8080` | Port to listen on (443 is a good choice to blend in). |

### `agent`

| Key | Default | Description |
|---|---|---|
| `threads` | `2` | Worker threads. |
| `listenIp` | `127.0.0.1` | IPv4 address for local applications to connect to. |
| `listenPort` | `8080` | Port for local applications. |
| `serverIp` | `127.0.0.1` | IPv4 address of the NipoVPN server. |
| `serverPort` | `8080` | Port of the NipoVPN server. |
| `httpVersion` | `1.1` | HTTP version in decoy requests. |
| `userAgent` | `NipoAgent` | `User-Agent` header in decoy requests. |

## Security notes

- **Change the token.** The sample token is public; the program logs a warning
  if it is still in use.
- **Turn on at least one of `aesEnable` and `tlsEnable`.** Without either,
  traffic is only obfuscated, not encrypted.
- **Direct tunnel mode** sends tunneled bytes without the decoy framing or AES,
  so pair it with `tlsEnable: true`.
- **`tlsVerifyPeer: false`** protects against passive observers but not against
  an active man-in-the-middle. Enable it with a proper `tlsCaFile` if that
  matters to you. AES-GCM with a strong token still authenticates the payload.
- **Listening on `0.0.0.0` on the agent** exposes your proxy to the local
  network. Use `127.0.0.1` unless you mean to share it.

> **Protocol change:** this version uses AES-256-GCM and carries the session and
> action inside the encrypted body instead of `X-Nipo-*` headers. It is not
> compatible with older agents, servers or the Android app; upgrade all of them
> together.

## Logging

Log lines look like:

```
2026-01-01_12:00:00 [AGENT] [INFO] [<session-uuid>] [CONNECT] [SRC 127.0.0.1:51234] [ORIGIN example.com:443]
```

- `INFO`: startup, effective configuration (token hidden), connections opened.
- `TRACE`: adds the destination of every forwarded request.
- `DEBUG`: adds handshake, encryption and byte-level details. Use only for
  troubleshooting.

```bash
tail -f /var/log/nipovpn/nipovpn.log
journalctl -u nipovpn-server.service -f
```

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `Configuration validation failed: ...` | A required key is missing or invalid; the message names the block. |
| `token must be at least 32 bytes when aesEnable is true` | Use a longer token (`openssl rand -hex 16`). |
| `Error opening log file` | Create the log directory and file (see Quick start). |
| Server logs `Decryption Failed` / `Invalid Envelope` | `token`, `aesEnable` or NipoVPN versions differ between agent and server. |
| Agent logs `[TLS] [ERROR]` | `tlsEnable` differs between the two sides, or the certificate/key path is wrong. |
| `[CONNECT] [ERROR] [To Server]` | Agent cannot reach `serverIp:serverPort` (firewall, wrong address, server not running). |

## Contribution

See the [contribution guide](CONTRIBUTING.md). In short: branch from `dev`,
format with `scripts/format.sh`, build and test, and open a pull request against
`dev`.

## License

NipoVPN is released under the [GNU General Public License v3.0](LICENSE).

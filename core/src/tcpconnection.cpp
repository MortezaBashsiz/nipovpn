#include "tcpconnection.hpp"

#include <openssl/ssl.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>
#include <vector>

#include "http_utils.hpp"
#include "usage.hpp"

TCPConnection::TCPConnection(boost::asio::io_context &io_context,
                             const std::shared_ptr<Config> &config,
                             const std::shared_ptr<Log> &log,
                             TCPClient::pointer client)
    : socket_(io_context),
      sslContext_(boost::asio::ssl::context::tls_server),
      tlsSocket_(nullptr),
      config_(config),
      log_(log),
      io_context_(io_context),
      client_(client),
      timeout_(io_context),
      pollTimer_(io_context),
      strand_(boost::asio::make_strand(io_context_)) {
    uuid_ = boost::uuids::random_generator()();
    end_ = false;
    connect_ = false;
    tunnelClosed_ = false;
    pollInProgress_ = false;
    pollDelayMs_ = 0;
}

namespace {
    constexpr unsigned int MaxPollDelayMs = 50;
}

std::mutex TCPConnection::registryMutex_;
std::unordered_map<std::string, std::vector<std::weak_ptr<TCPConnection>>> TCPConnection::byUser_;

void TCPConnection::trackUser(const std::string &user) {
    if (user.empty() || user == trackedUser_) return;
    trackedUser_ = user;
    usage_ = Usage::instance().counter(user);

    std::lock_guard<std::mutex> lock(registryMutex_);
    auto &list = byUser_[user];
    list.erase(std::remove_if(list.begin(), list.end(),
                              [](const std::weak_ptr<TCPConnection> &w) { return w.expired(); }),
               list.end());
    list.push_back(weak_from_this());
}

std::size_t TCPConnection::revokeUsers(const std::vector<std::string> &users) {
    std::vector<pointer> open;
    {
        std::lock_guard<std::mutex> lock(registryMutex_);
        for (const auto &name: users) {
            auto it = byUser_.find(name);
            if (it == byUser_.end()) continue;
            for (const auto &w: it->second) {
                if (auto conn = w.lock()) open.push_back(conn);
            }
            byUser_.erase(it);
        }
    }
    for (const auto &conn: open) {
        boost::asio::post(conn->strand_, [conn] { conn->socketShutdown(); });
    }
    return open.size();
}

boost::asio::ip::tcp::socket &TCPConnection::socket() { return socket_; }

TCPConnection::ssl_stream &TCPConnection::tlsSocket() { return *tlsSocket_; }

bool TCPConnection::initTlsServerContext() {
    try {
        sslContext_ = boost::asio::ssl::context(boost::asio::ssl::context::tls_server);

        sslContext_.set_options(
                boost::asio::ssl::context::default_workarounds |
                boost::asio::ssl::context::no_sslv2 |
                boost::asio::ssl::context::no_sslv3 |
                boost::asio::ssl::context::single_dh_use);

        SSL_CTX_set_min_proto_version(sslContext_.native_handle(), TLS1_2_VERSION);

        sslContext_.use_certificate_chain_file(config_->server().tlsCertFile);
        sslContext_.use_private_key_file(
                config_->server().tlsKeyFile,
                boost::asio::ssl::context::pem);

        if (!SSL_CTX_check_private_key(sslContext_.native_handle())) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection initTlsServerContext] private key does not match certificate",
                        Log::Level::ERROR);
            return false;
        }

        sslContext_.set_verify_mode(boost::asio::ssl::verify_none);

        if (SSL_CTX_set_cipher_list(
                    sslContext_.native_handle(),
                    "ECDHE-RSA-AES256-GCM-SHA384:"
                    "ECDHE-RSA-AES128-GCM-SHA256:"
                    "ECDHE-RSA-CHACHA20-POLY1305:"
                    "AES256-GCM-SHA384:"
                    "AES128-GCM-SHA256") != 1) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection initTlsServerContext] failed to set cipher list",
                        Log::Level::ERROR);
            return false;
        }

#if defined(TLS1_3_VERSION)
        SSL_CTX_set_ciphersuites(
                sslContext_.native_handle(),
                "TLS_AES_256_GCM_SHA384:"
                "TLS_AES_128_GCM_SHA256:"
                "TLS_CHACHA20_POLY1305_SHA256");
#endif

        tlsSocket_ = std::make_unique<ssl_stream>(io_context_, sslContext_);
        closed_ = false;

        return true;
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection initTlsServerContext] " +
                            error.what(),
                    Log::Level::ERROR);
        return false;
    }
}

bool TCPConnection::doHandshakeServer() {
    try {
        resetTimeout();
        tlsSocket_->handshake(boost::asio::ssl::stream_base::server);
        cancelTimeout();
        return true;
    } catch (std::exception &error) {
        cancelTimeout();
        log_->write("[" + to_string(uuid_) + "] [TCPConnection doHandshakeServer] " + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
        return false;
    }
}

void TCPConnection::startAgent() {
    std::lock_guard lock(mutex_);
    client_->uuid_ = uuid_;
    doReadAgent();
}

void TCPConnection::startServer() {
    std::lock_guard lock(mutex_);
    client_->uuid_ = uuid_;
    doReadServer();
}

void TCPConnection::doReadAgent() {
    try {
        readBuffer_.consume(readBuffer_.size());
        writeBuffer_.consume(writeBuffer_.size());

        resetTimeout();

        socket_.async_read_some(
                readBuffer_.prepare(BufferSize),
                boost::asio::bind_executor(
                        strand_,
                        [self = shared_from_this()](
                                const boost::system::error_code &ec,
                                std::size_t bytes) {
                            if (!ec) {
                                self->readBuffer_.commit(bytes);
                            }

                            self->handleReadAgent(ec, bytes);
                        }));

    } catch (std::exception &error) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [TCPConnection doReadAgent] [catch] " +
                        error.what(),
                Log::Level::ERROR);

        socketShutdown();
    }
}

void TCPConnection::doReadServer() {
    try {
        readBuffer_.consume(readBuffer_.size());
        writeBuffer_.consume(writeBuffer_.size());
        resetTimeout();

        auto handler = boost::asio::bind_executor(
                strand_,
                boost::bind(&TCPConnection::handleReadServer,
                            shared_from_this(),
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));

        if (config_->general().tlsEnable) {
            boost::asio::async_read_until(*tlsSocket_, readBuffer_, "\r\n\r\n", handler);
        } else {
            boost::asio::async_read_until(socket_, readBuffer_, "\r\n\r\n", handler);
        }
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) + "] [TCPConnection doReadServer] [catch] " + error.what(),
                    Log::Level::ERROR);
        socketShutdown();
    }
}

void TCPConnection::handleReadAgent(const boost::system::error_code &error, size_t) {
    try {
        cancelTimeout();

        if (error) {
            socketShutdown();
            return;
        }

        boost::system::error_code errorIn;

        if (config_->agent().protocol == "socks5") {
            if (!handleSocks5Handshake()) {
                socketShutdown();
                return;
            }

            if (udpAssociate_) {
                startUdpAssociate();
                return;
            }
        }

        std::string current = streambufToString(readBuffer_);

        if (current.find("\r\n\r\n") == std::string::npos) {
            boost::asio::read_until(socket_, readBuffer_, "\r\n\r\n", errorIn);
        }

        if (errorIn && errorIn != boost::asio::error::eof) {
            socketShutdown();
            return;
        }

        current = streambufToString(readBuffer_);

        const std::string headers = HttpUtils::extractHeaders(current);
        const std::string body = HttpUtils::extractBody(current);

        const bool isConnect =
                headers.rfind("CONNECT ", 0) == 0 ||
                headers.rfind("connect ", 0) == 0;

        if (!isConnect) {
            std::size_t contentLength = 0;

            if (HttpUtils::parseContentLength(headers, contentLength) &&
                body.size() < contentLength) {
                boost::asio::read(socket_,
                                  readBuffer_,
                                  boost::asio::transfer_exactly(contentLength - body.size()),
                                  errorIn);

                if (errorIn && errorIn != boost::asio::error::eof) {
                    socketShutdown();
                    return;
                }
            }
        }

        log_->write("[" + to_string(uuid_) + "] [TCPConnection handleReadAgent] [SRC " +
                            socket_.remote_endpoint().address().to_string() + ":" +
                            std::to_string(socket_.remote_endpoint().port()) + "] [Bytes " +
                            std::to_string(readBuffer_.size()) + "] ",
                    Log::Level::DEBUG);

        AgentHandler::pointer agentHandler_ = AgentHandler::create(
                readBuffer_,
                writeBuffer_,
                config_,
                log_,
                client_,
                socket_.remote_endpoint().address().to_string() + ":" +
                        std::to_string(socket_.remote_endpoint().port()),
                uuid_);

        agentHandler_->handle();

        end_ = agentHandler_->end_;
        connect_ = agentHandler_->connect_;

        if (config_->agent().protocol == "socks5" && connect_) {
            writeBuffer_.consume(writeBuffer_.size());

            const unsigned char ok[10] = {
                    0x05, 0x00, 0x00, 0x01,
                    0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00};

            std::ostream os(&writeBuffer_);
            os.write(reinterpret_cast<const char *>(ok), sizeof(ok));
        }

        if (writeBuffer_.size() == 0) {
            socketShutdown();
            return;
        }

        resetTimeout();

        boost::asio::async_write(
                socket_,
                writeBuffer_.data(),
                boost::asio::bind_executor(
                        strand_,
                        [self = shared_from_this()](const boost::system::error_code &ec, std::size_t) {
                            self->cancelTimeout();

                            if (ec) {
                                self->socketShutdown();
                                return;
                            }

                            if (self->connect_) {
                                self->tunnelClosed_ = false;

                                if (self->config_->general().tunnelEnable) {
                                    self->enableDirectTunnelMode();
                                    self->relayClientToServer();
                                    self->relayServerToClient();
                                } else {
                                    if (!self->config_->agent().connectionReuse) {
                                        self->client_->socketShutdown();
                                    }

                                    self->pollDelayMs_ = self->config_->agent().pullTimeout;
                                    self->startTunnelReadFromClient();
                                    self->startTunnelPollServer();
                                }

                                return;
                            }

                            self->doReadAgent();
                        }));
    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) + "] [TCPConnection handleReadAgent] [catch read] " + error.what(),
                    Log::Level::DEBUG);
        socketShutdown();
    }
}

void TCPConnection::handleReadServer(const boost::system::error_code &error, size_t) {
    try {
        cancelTimeout();

        if (error) {
            socketShutdown();
            return;
        }

        boost::system::error_code bodyError;
        std::string remotePeer;

        if (config_->general().tlsEnable) {
            if (!HttpUtils::readRemainingHttpBody(*tlsSocket_, readBuffer_, bodyError)) {
                socketShutdown();
                return;
            }

            remotePeer =
                    tlsSocket_->lowest_layer().remote_endpoint().address().to_string() +
                    ":" +
                    std::to_string(tlsSocket_->lowest_layer().remote_endpoint().port());
        } else {
            if (!HttpUtils::readRemainingHttpBody(socket_, readBuffer_, bodyError)) {
                socketShutdown();
                return;
            }

            remotePeer =
                    socket_.remote_endpoint().address().to_string() +
                    ":" +
                    std::to_string(socket_.remote_endpoint().port());
        }

        log_->write(
                "[" + to_string(uuid_) + "] [TCPConnection handleReadServer] [SRC " +
                        remotePeer +
                        "] [Bytes " +
                        std::to_string(readBuffer_.size()) +
                        "] ",
                Log::Level::DEBUG);

        ServerHandler::pointer serverHandler_ = ServerHandler::create(
                readBuffer_,
                writeBuffer_,
                config_,
                log_,
                client_,
                remotePeer,
                uuid_,
                userHint_);

        serverHandler_->handle();
        trackUser(serverHandler_->userName());

        end_ = serverHandler_->end_;
        connect_ = serverHandler_->connect_;

        if (writeBuffer_.size() == 0) {
            socketShutdown();
            return;
        }

        resetTimeout();

        auto writeHandler =
                [self = shared_from_this(), keepAlive = serverHandler_->keepAlive_,
                 udp = serverHandler_->udp_, user = serverHandler_->userName()](
                        const boost::system::error_code &ec,
                        std::size_t) {
                    self->cancelTimeout();

                    if (ec) {
                        self->socketShutdown();
                        return;
                    }

                    if (udp) {
                        self->startServerUdpStream(user);
                        return;
                    }

                    if (self->connect_ && self->config_->general().tunnelEnable) {
                        self->enableDirectTunnelMode();
                        self->relayAgentToTarget();
                        self->relayTargetToAgent();
                        return;
                    }

                    if (keepAlive) {
                        self->doReadServer();
                    } else {
                        self->socketShutdown();
                    }
                };

        if (config_->general().tlsEnable) {
            boost::asio::async_write(
                    *tlsSocket_,
                    writeBuffer_.data(),
                    boost::asio::bind_executor(strand_, writeHandler));
        } else {
            boost::asio::async_write(
                    socket_,
                    writeBuffer_.data(),
                    boost::asio::bind_executor(strand_, writeHandler));
        }
    } catch (std::exception &error) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [TCPConnection handleReadServer] [catch read] " +
                        error.what(),
                Log::Level::DEBUG);

        socketShutdown();
    }
}

std::optional<std::string> TCPConnection::postTunnelAction(const std::string &action,
                                                           const std::string &rawBody) {
    std::lock_guard<std::mutex> relayLock(relayMutex_);
    client_->uuid_ = uuid_;
    return postRelayAction(*client_, config_, log_, uuid_, action, rawBody);
}

bool TCPConnection::readExactFromClient(void *data, std::size_t size) {
    auto *out = static_cast<char *>(data);
    std::size_t copied = 0;

    while (copied < size && readBuffer_.size() > 0) {
        std::size_t n = std::min(size - copied, readBuffer_.size());

        auto buffers = readBuffer_.data();
        std::size_t copiedNow = boost::asio::buffer_copy(
                boost::asio::buffer(out + copied, n),
                buffers);

        readBuffer_.consume(copiedNow);
        copied += copiedNow;
    }

    if (copied == size) {
        return true;
    }

    boost::system::error_code ec;
    boost::asio::read(
            socket_,
            boost::asio::buffer(out + copied, size - copied),
            ec);

    return !ec;
}

bool TCPConnection::handleSocks5Handshake() {

    log_->write(
            "[" + to_string(uuid_) + "] [TCPConnection handleSocks5Handshake] handshake start",
            Log::Level::DEBUG);

    unsigned char header[2];
    if (!readExactFromClient(header, 2)) {
        log_->write(
                "[" + to_string(uuid_) + "] [TCPConnection handleSocks5Handshake] [SOCKS5] version=" +
                        std::to_string(header[0]),
                Log::Level::DEBUG);

        return false;
    }

    if (header[0] != 0x05) {
        log_->write("[" + to_string(uuid_) + "] [SOCKS5] invalid version", Log::Level::DEBUG);
        return false;
    }

    std::vector<unsigned char> methods(header[1]);
    if (!readExactFromClient(methods.data(), methods.size())) return false;

    const unsigned char authReply[2] = {0x05, 0x00};
    boost::asio::write(socket_, boost::asio::buffer(authReply, 2));
    log_->write(
            "[" + to_string(uuid_) + "] [TCPConnection handleSocks5Handshake] auth reply sent" +
                    std::to_string(header[0]),
            Log::Level::DEBUG);

    unsigned char req[4];
    if (!readExactFromClient(req, 4)) return false;

    const auto &general = config_->general();
    const bool commandAllowed = (req[1] == 0x01 && general.allowsTcp()) ||
                                (req[1] == 0x03 && general.allowsUdp());
    if (req[0] == 0x05 && (req[1] == 0x01 || req[1] == 0x03) && !commandAllowed) {
        log_->write("[" + to_string(uuid_) + "] [SOCKS5] " +
                            (req[1] == 0x01 ? "CONNECT" : "UDP ASSOCIATE") +
                            " refused: general.traffic is " + general.traffic,
                    Log::Level::INFO);
        const unsigned char fail[10] = {0x05, static_cast<unsigned char>(req[1] == 0x01 ? 0x02 : 0x07),
                                        0x00, 0x01, 0, 0, 0, 0, 0, 0};
        boost::system::error_code ignored;
        boost::asio::write(socket_, boost::asio::buffer(fail, 10), ignored);
        return false;
    }

    if (req[0] != 0x05 || (req[1] != 0x01 && req[1] != 0x03)) {
        const unsigned char fail[10] = {0x05, 0x07, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
        boost::asio::write(socket_, boost::asio::buffer(fail, 10));
        return false;
    }

    std::string host;

    if (req[3] == 0x01) {
        unsigned char ip[4];
        if (!readExactFromClient(ip, 4)) return false;

        host =
                std::to_string(ip[0]) + "." +
                std::to_string(ip[1]) + "." +
                std::to_string(ip[2]) + "." +
                std::to_string(ip[3]);
    } else if (req[3] == 0x03) {
        unsigned char len;
        if (!readExactFromClient(&len, 1)) return false;

        std::vector<char> domain(len);
        if (!readExactFromClient(domain.data(), domain.size())) return false;

        host.assign(domain.begin(), domain.end());
    } else if (req[3] == 0x04) {
        unsigned char ip[16];
        if (!readExactFromClient(ip, 16)) return false;

        boost::asio::ip::address_v6::bytes_type bytes;
        std::copy(ip, ip + 16, bytes.begin());
        host = boost::asio::ip::address_v6(bytes).to_string();
    } else {
        return false;
    }

    unsigned char portBytes[2];
    if (!readExactFromClient(portBytes, 2)) return false;

    unsigned short port =
            static_cast<unsigned short>((portBytes[0] << 8) | portBytes[1]);

    if (req[1] == 0x03) {
        udpAssociate_ = true;
        return true;
    }

    std::string target;

    if (host.find(':') != std::string::npos) {
        target = "[" + host + "]:" + std::to_string(port);
    } else {
        target = host + ":" + std::to_string(port);
    }

    std::string connectRequest =
            "CONNECT " + target +
            " HTTP/1.1\r\n"
            "Host: " +
            target +
            "\r\n"
            "\r\n";

    copyStringToStreambuf(connectRequest, readBuffer_);

    return true;
}

void TCPConnection::startTunnelReadFromClient() {
    if (tunnelClosed_ || !socket_.is_open()) return;

    socket_.async_read_some(
            boost::asio::buffer(downstreamBuf_),
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](const boost::system::error_code &ec, std::size_t bytes) {
                        if (ec || bytes == 0) {
                            self->closeTunnelSession();
                            self->socketShutdown();
                            return;
                        }

                        std::string raw(self->downstreamBuf_.data(), bytes);

                        if (!self->postTunnelAction("send", raw)) {
                            self->abortTunnel("send refused or failed");
                            return;
                        }

                        self->pollDelayMs_ = self->config_->agent().pullTimeout;
                        if (self->pollInProgress_) self->pollTimer_.cancel();

                        self->startTunnelReadFromClient();
                    }));
}

void TCPConnection::startTunnelPollServer() {
    if (tunnelClosed_ || !socket_.is_open() || pollInProgress_) return;

    pollInProgress_ = true;

    pollTimer_.expires_after(std::chrono::milliseconds(pollDelayMs_));
    pollTimer_.async_wait(
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](const boost::system::error_code &ec) {
                        self->pollInProgress_ = false;

                        if (self->tunnelClosed_ || !self->socket_.is_open()) return;

                        if (ec == boost::asio::error::operation_aborted) {
                            self->startTunnelPollServer();
                            return;
                        }

                        if (ec) return;

                        std::optional<std::string> reply = self->postTunnelAction("recv", "");
                        if (!reply) {
                            self->abortTunnel("recv refused or failed");
                            return;
                        }
                        std::string data = std::move(*reply);

                        const unsigned int baseDelay = self->config_->agent().pullTimeout;
                        self->pollDelayMs_ =
                                data.empty()
                                        ? std::min(std::max(self->pollDelayMs_ * 2, 1u), std::max(MaxPollDelayMs, baseDelay))
                                        : baseDelay;

                        if (!data.empty()) {
                            boost::asio::async_write(
                                    self->socket_,
                                    boost::asio::buffer(data),
                                    boost::asio::bind_executor(
                                            self->strand_,
                                            [self](const boost::system::error_code &werr, std::size_t) {
                                                if (werr) {
                                                    self->closeTunnelSession();
                                                    self->socketShutdown();
                                                    return;
                                                }

                                                self->startTunnelPollServer();
                                            }));
                            return;
                        }

                        self->startTunnelPollServer();
                    }));
}

void TCPConnection::abortTunnel(const std::string &reason) {
    if (tunnelClosed_) return;

    log_->write("[" + to_string(uuid_) + "] [TCPConnection] [Tunnel closed] " + reason,
                Log::Level::INFO);

    tunnelClosed_ = true;
    pollTimer_.cancel();
    socketShutdown();
}

void TCPConnection::closeTunnelSession() {
    if (tunnelClosed_) return;

    tunnelClosed_ = true;
    pollTimer_.cancel();

    postTunnelAction("close", "");
}

void TCPConnection::resetTimeout() {
    if (!config_->general().timeout) return;
    if (closed_) return;

    timeout_.cancel();

    timeout_.expires_after(std::chrono::seconds(config_->general().timeout));

    timeout_.async_wait(
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](const boost::system::error_code &error) {
                        self->onTimeout(error);
                    }));
}

void TCPConnection::cancelTimeout() {
    if (!config_->general().timeout) return;

    boost::system::error_code ignored;
    timeout_.cancel();
}

void TCPConnection::onTimeout(const boost::system::error_code &error) {
    if (error == boost::asio::error::operation_aborted) return;
    if (error) return;
    if (connect_) return;
    if (closed_) return;

    log_->write("[" + to_string(uuid_) +
                        "] [TCPConnection onTimeout] [expiration] " +
                        std::to_string(+config_->general().timeout) +
                        " seconds has passed, and the timeout has expired",
                Log::Level::TRACE);

    socketShutdown();
}

void TCPConnection::socketShutdown() {
    if (closed_.exchange(true)) {
        return;
    }

    boost::system::error_code ignored;

    try {
        if (udp_) udp_->close();
        if (udpSession_) udpSession_->close();

        pollTimer_.cancel();
        ignored.clear();

        timeout_.cancel();
        ignored.clear();

        if (tlsSocket_) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection socketShutdown] [SSL]",
                        Log::Level::DEBUG);

            auto &lowest = tlsSocket_->lowest_layer();

            lowest.cancel(ignored);
            ignored.clear();

            if (lowest.is_open()) {
                lowest.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
                ignored.clear();

                lowest.close(ignored);
                ignored.clear();
            }
        }

        if (socket_.is_open()) {
            log_->write("[" + to_string(uuid_) +
                                "] [TCPConnection socketShutdown] [TCP]",
                        Log::Level::DEBUG);

            socket_.cancel(ignored);
            ignored.clear();

            socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
            ignored.clear();

            socket_.close(ignored);
            ignored.clear();
        }

        if (client_) {
            client_->socketShutdown();
        }

    } catch (std::exception &error) {
        log_->write("[" + to_string(uuid_) +
                            "] [TCPConnection socketShutdown] [catch] " +
                            error.what(),
                    Log::Level::DEBUG);
    }
}
void TCPConnection::enableDirectTunnelMode() {
    tunnelMode_ = true;
}

void TCPConnection::asyncWriteToAgentConnection(
        const char *data,
        std::size_t bytes,
        std::function<void(const boost::system::error_code &)> done) {
    if (config_->general().tlsEnable && tlsSocket_) {
        boost::asio::async_write(
                *tlsSocket_,
                boost::asio::buffer(data, bytes),
                boost::asio::bind_executor(
                        strand_,
                        [done](const boost::system::error_code &ec, std::size_t) {
                            done(ec);
                        }));
        return;
    }

    boost::asio::async_write(
            socket_,
            boost::asio::buffer(data, bytes),
            boost::asio::bind_executor(
                    strand_,
                    [done](const boost::system::error_code &ec, std::size_t) {
                        done(ec);
                    }));
}

void TCPConnection::asyncReadFromAgentConnection(
        std::array<char, BufferSize> &buffer,
        std::function<void(const boost::system::error_code &, std::size_t)> done) {
    if (config_->general().tlsEnable && tlsSocket_) {
        tlsSocket_->async_read_some(
                boost::asio::buffer(buffer),
                boost::asio::bind_executor(strand_, done));
        return;
    }

    socket_.async_read_some(
            boost::asio::buffer(buffer),
            boost::asio::bind_executor(strand_, done));
}

void TCPConnection::asyncWriteToAgentServerConnection(
        const char *data,
        std::size_t bytes,
        std::function<void(const boost::system::error_code &)> done) {
    if (client_->tlsEnabled()) {
        boost::asio::async_write(
                client_->sslSocket(),
                boost::asio::buffer(data, bytes),
                boost::asio::bind_executor(
                        strand_,
                        [done](const boost::system::error_code &ec, std::size_t) {
                            done(ec);
                        }));
        return;
    }

    boost::asio::async_write(
            client_->socket(),
            boost::asio::buffer(data, bytes),
            boost::asio::bind_executor(
                    strand_,
                    [done](const boost::system::error_code &ec, std::size_t) {
                        done(ec);
                    }));
}

void TCPConnection::asyncReadFromAgentServerConnection(
        std::array<char, BufferSize> &buffer,
        std::function<void(const boost::system::error_code &, std::size_t)> done) {
    if (client_->tlsEnabled()) {
        client_->sslSocket().async_read_some(
                boost::asio::buffer(buffer),
                boost::asio::bind_executor(strand_, done));
        return;
    }

    client_->socket().async_read_some(
            boost::asio::buffer(buffer),
            boost::asio::bind_executor(strand_, done));
}

void TCPConnection::relayClientToServer() {
    if (tunnelClosed_ || !socket_.is_open() || !client_->isOpen()) {
        socketShutdown();
        return;
    }

    socket_.async_read_some(
            boost::asio::buffer(downstreamBuf_),
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](
                            const boost::system::error_code &ec,
                            std::size_t bytes) {
                        if (ec || bytes == 0) {
                            self->socketShutdown();
                            return;
                        }

                        self->asyncWriteToAgentServerConnection(
                                self->downstreamBuf_.data(),
                                bytes,
                                [self](const boost::system::error_code &werr) {
                                    if (werr) {
                                        self->socketShutdown();
                                        return;
                                    }

                                    self->relayClientToServer();
                                });
                    }));
}

void TCPConnection::relayServerToClient() {
    if (tunnelClosed_ || !socket_.is_open() || !client_->isOpen()) {
        socketShutdown();
        return;
    }

    asyncReadFromAgentServerConnection(
            upstreamBuf_,
            [self = shared_from_this()](
                    const boost::system::error_code &ec,
                    std::size_t bytes) {
                if (ec || bytes == 0) {
                    self->socketShutdown();
                    return;
                }

                boost::asio::async_write(
                        self->socket_,
                        boost::asio::buffer(self->upstreamBuf_.data(), bytes),
                        boost::asio::bind_executor(
                                self->strand_,
                                [self](const boost::system::error_code &werr, std::size_t) {
                                    if (werr) {
                                        self->socketShutdown();
                                        return;
                                    }

                                    self->relayServerToClient();
                                }));
            });
}

void TCPConnection::relayAgentToTarget() {
    if (tunnelClosed_ || !client_->isOpen()) {
        socketShutdown();
        return;
    }

    asyncReadFromAgentConnection(
            downstreamBuf_,
            [self = shared_from_this()](
                    const boost::system::error_code &ec,
                    std::size_t bytes) {
                if (ec || bytes == 0) {
                    self->socketShutdown();
                    return;
                }

                Usage::instance().add(self->usage_, bytes);

                boost::asio::async_write(
                        self->client_->socket(),
                        boost::asio::buffer(self->downstreamBuf_.data(), bytes),
                        boost::asio::bind_executor(
                                self->strand_,
                                [self](const boost::system::error_code &werr, std::size_t) {
                                    if (werr) {
                                        self->socketShutdown();
                                        return;
                                    }

                                    self->relayAgentToTarget();
                                }));
            });
}

void TCPConnection::relayTargetToAgent() {
    if (tunnelClosed_ || !client_->isOpen()) {
        socketShutdown();
        return;
    }

    client_->socket().async_read_some(
            boost::asio::buffer(upstreamBuf_),
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](
                            const boost::system::error_code &ec,
                            std::size_t bytes) {
                        if (ec || bytes == 0) {
                            self->socketShutdown();
                            return;
                        }

                        Usage::instance().add(self->usage_, bytes);

                        self->asyncWriteToAgentConnection(
                                self->upstreamBuf_.data(),
                                bytes,
                                [self](const boost::system::error_code &werr) {
                                    if (werr) {
                                        self->socketShutdown();
                                        return;
                                    }

                                    self->relayTargetToAgent();
                                });
                    }));
}

std::string TCPConnection::HttpUtils::toLowerCopy(std::string s) {
    return http_utils::toLowerCopy(std::move(s));
}

std::string TCPConnection::HttpUtils::extractHeaders(const std::string &msg) {
    return http_utils::extractHeaders(msg);
}

std::string TCPConnection::HttpUtils::extractBody(const std::string &msg) {
    return http_utils::extractBody(msg);
}

bool TCPConnection::HttpUtils::parseContentLength(const std::string &headers, std::size_t &value) {
    return http_utils::parseContentLength(headers, value);
}

bool TCPConnection::HttpUtils::isChunked(const std::string &headers) {
    return http_utils::isChunked(headers);
}

bool TCPConnection::HttpUtils::readRemainingHttpBody(
        boost::asio::ip::tcp::socket &stream,
        boost::asio::streambuf &buf,
        boost::system::error_code &ec) {
    return readRemainingHttpBodyImpl(
            buf,
            ec,
            [&stream](boost::asio::mutable_buffer buffer, boost::system::error_code &error) {
                return stream.read_some(buffer, error);
            });
}

bool TCPConnection::HttpUtils::readRemainingHttpBody(
        ssl_stream &stream,
        boost::asio::streambuf &buf,
        boost::system::error_code &ec) {
    return readRemainingHttpBodyImpl(
            buf,
            ec,
            [&stream](boost::asio::mutable_buffer buffer, boost::system::error_code &error) {
                return stream.read_some(buffer, error);
            });
}

bool TCPConnection::HttpUtils::readRemainingHttpBodyImpl(
        boost::asio::streambuf &buf,
        boost::system::error_code &ec,
        const std::function<std::size_t(boost::asio::mutable_buffer,
                                        boost::system::error_code &)> &readSome) {
    std::string current = streambufToString(buf);
    std::string headers = extractHeaders(current);
    std::string body = extractBody(current);

    std::size_t contentLength = 0;

    if (parseContentLength(headers, contentLength)) {
        if (body.size() < contentLength) {
            std::vector<char> tmp(contentLength - body.size());
            std::size_t total = 0;

            while (total < tmp.size()) {
                std::size_t n = readSome(
                        boost::asio::buffer(tmp.data() + total, tmp.size() - total),
                        ec);

                if (ec) return false;
                total += n;
            }

            std::ostream os(&buf);
            os.write(tmp.data(), static_cast<std::streamsize>(tmp.size()));
        }

        return true;
    }

    if (isChunked(headers)) {
        for (;;) {
            current = streambufToString(buf);

            auto pos = current.find("\r\n\r\n");
            if (pos == std::string::npos) return false;

            auto bodyPart = current.substr(pos + 4);

            if (bodyPart.find("\r\n0\r\n\r\n") != std::string::npos ||
                bodyPart == "0\r\n\r\n") {
                return true;
            }

            std::array<char, 4096> tmp{};
            std::size_t n = readSome(boost::asio::buffer(tmp), ec);

            if (ec) return false;

            std::ostream os(&buf);
            os.write(tmp.data(), static_cast<std::streamsize>(n));
        }
    }

    return true;
}
void TCPConnection::startUdpAssociate() {
    boost::system::error_code ec;
    const auto local = socket_.local_endpoint(ec);
    if (ec) {
        socketShutdown();
        return;
    }
    const auto peer = socket_.remote_endpoint(ec);
    if (ec) {
        socketShutdown();
        return;
    }

    udp_ = UdpAssociation::create(io_context_, config_, log_,
                                  TCPClient::create(io_context_, config_, log_),
                                  uuid_, peer.address(), local.address());

    if (!udp_->open()) {
        const unsigned char fail[10] = {0x05, 0x01, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
        boost::asio::write(socket_, boost::asio::buffer(fail), ec);
        udp_.reset();
        socketShutdown();
        return;
    }

    std::string reply("\x05\x00\x00", 3);
    reply += udp_relay::encodeSocksAddress(udp_->localEndpoint());
    boost::asio::write(socket_, boost::asio::buffer(reply), ec);
    if (ec) {
        socketShutdown();
        return;
    }

    connect_ = true;
    udp_->start();
    watchUdpControl();
}

void TCPConnection::watchUdpControl() {
    socket_.async_read_some(
            boost::asio::buffer(downstreamBuf_),
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](const boost::system::error_code &ec, std::size_t) {
                        if (ec) {
                            self->socketShutdown();
                            return;
                        }
                        self->watchUdpControl();
                    }));
}

void TCPConnection::startServerUdpStream(const std::string &user) {
    connect_ = true;
    tunnelMode_ = true;

    udpSession_ = UdpSession::create(io_context_, config_, log_, uuid_, user, usage_);

    std::weak_ptr<TCPConnection> weak = shared_from_this();
    udpSession_->start([weak](std::string body) {
        auto self = weak.lock();
        if (!self) return;
        boost::asio::post(self->strand_, [self, body = std::move(body)]() mutable {
            self->queueUdpToAgent(std::move(body));
        });
    });

    readUdpFromAgent();
}

void TCPConnection::readUdpFromAgent() {
    if (closed_) return;

    asyncReadFromAgentConnection(
            downstreamBuf_,
            [self = shared_from_this()](const boost::system::error_code &ec, std::size_t bytes) {
                if (ec || bytes == 0) {
                    self->socketShutdown();
                    return;
                }

                self->udpPending_.append(self->downstreamBuf_.data(), bytes);
                bool ok = true;
                auto frames = udp_relay::takeFrames(self->udpPending_, ok);
                if (!ok) {
                    self->log_->write("[" + to_string(self->uuid_) + "] [UDP] [Closed] malformed frame from agent",
                                      Log::Level::INFO);
                    self->socketShutdown();
                    return;
                }
                for (auto &body: frames) self->udpSession_->send(std::move(body));

                self->readUdpFromAgent();
            });
}

void TCPConnection::queueUdpToAgent(std::string body) {
    if (closed_) return;

    std::string frame;
    if (!udp_relay::appendFrame(frame, body)) return;

    constexpr std::size_t MaxUdpWriteBytes = 4 * 1024 * 1024;
    if (udpWriteBytes_ + frame.size() > MaxUdpWriteBytes) return;

    udpWriteBytes_ += frame.size();
    udpWriteQueue_.push_back(std::move(frame));
    if (udpWriteQueue_.size() == 1) writeUdpNext();
}

void TCPConnection::writeUdpNext() {
    if (closed_ || udpWriteQueue_.empty()) return;

    asyncWriteToAgentConnection(
            udpWriteQueue_.front().data(),
            udpWriteQueue_.front().size(),
            [self = shared_from_this()](const boost::system::error_code &ec) {
                if (ec) {
                    self->socketShutdown();
                    return;
                }
                self->udpWriteBytes_ -= self->udpWriteQueue_.front().size();
                self->udpWriteQueue_.pop_front();
                self->writeUdpNext();
            });
}

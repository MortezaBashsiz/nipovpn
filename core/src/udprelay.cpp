#include "udprelay.hpp"

#include <algorithm>

#include "agenthandler.hpp"

namespace {
    constexpr auto UdpIdleTimeout = std::chrono::seconds(120);
    constexpr auto UdpIdleCheck = std::chrono::seconds(30);

    constexpr std::size_t MaxQueuedDatagrams = 1024;
    constexpr std::size_t MaxQueuedBytes = 4 * 1024 * 1024;

    constexpr std::size_t MaxWriteQueueBytes = 4 * 1024 * 1024;

    constexpr std::size_t MaxSendBatch = 32 * 1024;

    constexpr std::size_t MaxResolvedCache = 256;

    constexpr unsigned int MaxPollDelayMs = 50;

    constexpr int UdpSocketBuffer = 4 * 1024 * 1024;

    void enlargeBuffers(boost::asio::ip::udp::socket &socket) {
        boost::system::error_code ignored;
        socket.set_option(boost::asio::socket_base::receive_buffer_size(UdpSocketBuffer), ignored);
        socket.set_option(boost::asio::socket_base::send_buffer_size(UdpSocketBuffer), ignored);
    }

    std::int64_t nowTicks() {
        return std::chrono::steady_clock::now().time_since_epoch().count();
    }
}// namespace


namespace udp_relay {
    std::size_t socksAddressLength(const std::string &data, std::size_t offset) {
        if (offset >= data.size()) return 0;

        std::size_t need = 0;
        switch (static_cast<unsigned char>(data[offset])) {
            case 0x01:
                need = 1 + 4 + 2;
                break;
            case 0x03: {
                if (offset + 1 >= data.size()) return 0;
                const auto len = static_cast<unsigned char>(data[offset + 1]);
                if (len == 0) return 0;
                need = 1 + 1 + len + 2;
                break;
            }
            case 0x04:
                need = 1 + 16 + 2;
                break;
            default:
                return 0;
        }

        return data.size() - offset < need ? 0 : need;
    }

    std::string encodeSocksAddress(const boost::asio::ip::udp::endpoint &endpoint) {
        auto address = endpoint.address();
        if (address.is_v6() && address.to_v6().is_v4_mapped()) {
            address = boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped, address.to_v6());
        }

        std::string out;
        if (address.is_v4()) {
            const auto bytes = address.to_v4().to_bytes();
            out.push_back('\x01');
            out.append(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        } else {
            const auto bytes = address.to_v6().to_bytes();
            out.push_back('\x04');
            out.append(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        }
        out.push_back(static_cast<char>((endpoint.port() >> 8) & 0xff));
        out.push_back(static_cast<char>(endpoint.port() & 0xff));
        return out;
    }

    bool appendFrame(std::string &out, const std::string &body) {
        if (body.empty() || body.size() > MaxDatagram) return false;
        out.push_back(static_cast<char>((body.size() >> 8) & 0xff));
        out.push_back(static_cast<char>(body.size() & 0xff));
        out.append(body);
        return true;
    }

    std::vector<std::string> takeFrames(std::string &buffer, bool &ok) {
        ok = true;
        std::vector<std::string> frames;
        std::size_t pos = 0;

        while (buffer.size() - pos >= FrameHeader) {
            const std::size_t len =
                    (static_cast<unsigned char>(buffer[pos]) << 8) |
                    static_cast<unsigned char>(buffer[pos + 1]);
            if (len == 0) {
                ok = false;
                break;
            }
            if (buffer.size() - pos - FrameHeader < len) break;

            std::string body = buffer.substr(pos + FrameHeader, len);
            pos += FrameHeader + len;

            if (socksAddressLength(body, 0) == 0) {
                ok = false;
                break;
            }
            frames.push_back(std::move(body));
        }

        buffer.erase(0, pos);
        return frames;
    }

    std::optional<std::string> socksPacketToBody(const std::string &packet) {
        if (packet.size() < 3 || packet[2] != '\0') return std::nullopt;

        std::string body = packet.substr(3);
        if (socksAddressLength(body, 0) == 0 || body.size() > MaxDatagram) return std::nullopt;
        return body;
    }
}// namespace udp_relay


std::mutex UdpSession::registryMutex_;
std::unordered_map<std::string, UdpSession::pointer> UdpSession::registry_;

UdpSession::UdpSession(boost::asio::io_context &io_context,
                       const std::shared_ptr<Config> &config,
                       const std::shared_ptr<Log> &log,
                       boost::uuids::uuid uuid,
                       std::string user,
                       std::shared_ptr<Usage::Counter> usage)
    : io_context_(io_context),
      config_(config),
      log_(log),
      uuid_(uuid),
      user_(std::move(user)),
      usage_(std::move(usage)),
      strand_(boost::asio::make_strand(io_context)),
      idleTimer_(io_context),
      lastActivity_(nowTicks()) {}

void UdpSession::touch() { lastActivity_ = nowTicks(); }

bool UdpSession::idle(std::chrono::steady_clock::time_point now) const {
    const auto last = std::chrono::steady_clock::time_point(
            std::chrono::steady_clock::duration(lastActivity_.load()));
    return now - last > UdpIdleTimeout;
}

void UdpSession::start(Sink sink) {
    auto self = shared_from_this();
    boost::asio::post(strand_, [self, sink = std::move(sink)]() mutable {
        self->sink_ = std::move(sink);
        self->touch();
        if (!self->sink_) self->armIdleTimer();
    });
}

void UdpSession::armIdleTimer() {
    idleTimer_.expires_after(UdpIdleCheck);
    idleTimer_.async_wait(boost::asio::bind_executor(
            strand_, [self = shared_from_this()](const boost::system::error_code &ec) {
                if (ec || self->closed_) return;
                if (self->idle(std::chrono::steady_clock::now())) {
                    self->log_->write("[" + to_string(self->uuid_) + "] [UDP] [Session idle, closed]",
                                      Log::Level::DEBUG);
                    self->close();
                    return;
                }
                self->armIdleTimer();
            }));
}

void UdpSession::send(std::string body) {
    if (closed_) return;
    boost::asio::post(strand_, [self = shared_from_this(), body = std::move(body)] {
        self->doSend(body);
    });
}

void UdpSession::doSend(const std::string &body) {
    if (closed_) return;

    const std::size_t addressLength = udp_relay::socksAddressLength(body, 0);
    if (addressLength == 0) return;

    const auto target = resolve(body, addressLength);
    if (!target) return;

    Socket *s = socketFor(*target);
    if (!s) return;

    const std::size_t payload = body.size() - addressLength;
    boost::system::error_code ec;
    s->socket.send_to(boost::asio::buffer(body.data() + addressLength, payload), *target, 0, ec);
    if (ec) {
        log_->write("[" + to_string(uuid_) + "] [UDP] [Send Failed] [DST " +
                            target->address().to_string() + ":" + std::to_string(target->port()) +
                            "] " + ec.message(),
                    Log::Level::DEBUG);
        return;
    }

    Usage::instance().add(usage_, payload);
    touch();
}

std::optional<boost::asio::ip::udp::endpoint> UdpSession::resolve(const std::string &body,
                                                                  std::size_t addressLength) {
    const auto *p = reinterpret_cast<const unsigned char *>(body.data());
    const unsigned short port =
            static_cast<unsigned short>((p[addressLength - 2] << 8) | p[addressLength - 1]);

    switch (p[0]) {
        case 0x01: {
            boost::asio::ip::address_v4::bytes_type bytes;
            std::copy(p + 1, p + 5, bytes.begin());
            return udp::endpoint(boost::asio::ip::address_v4(bytes), port);
        }
        case 0x04: {
            boost::asio::ip::address_v6::bytes_type bytes;
            std::copy(p + 1, p + 17, bytes.begin());
            return udp::endpoint(boost::asio::ip::address_v6(bytes), port);
        }
        case 0x03: {
            const std::string host(body.data() + 2, p[1]);
            const std::string key = host + ":" + std::to_string(port);

            auto cached = resolved_.find(key);
            if (cached != resolved_.end()) return cached->second;

            boost::system::error_code ec;
            udp::resolver resolver(io_context_);
            auto results = resolver.resolve(host, std::to_string(port), ec);
            if (ec || results.empty()) {
                log_->write("[" + to_string(uuid_) + "] [UDP] [ERROR] [Resolving Host] [DST " + key + "]",
                            Log::Level::INFO);
                return std::nullopt;
            }

            udp::endpoint chosen = results.begin()->endpoint();
            for (const auto &entry: results) {
                if (entry.endpoint().address().is_v4()) {
                    chosen = entry.endpoint();
                    break;
                }
            }

            if (resolved_.size() >= MaxResolvedCache) resolved_.clear();
            resolved_[key] = chosen;
            return chosen;
        }
        default:
            return std::nullopt;
    }
}

UdpSession::Socket *UdpSession::socketFor(const udp::endpoint &target) {
    const bool v4 = target.address().is_v4();
    auto &slot = v4 ? v4_ : v6_;
    if (slot) return slot.get();

    auto s = std::make_unique<Socket>(io_context_);
    boost::system::error_code ec;
    s->socket.open(v4 ? udp::v4() : udp::v6(), ec);
    if (!ec) s->socket.bind(udp::endpoint(v4 ? udp::v4() : udp::v6(), 0), ec);
    if (!ec) enlargeBuffers(s->socket);
    if (ec) {
        log_->write("[" + to_string(uuid_) + "] [UDP] [Socket Failed] " + ec.message(),
                    Log::Level::INFO);
        return nullptr;
    }

    log_->write("[" + to_string(uuid_) + "] [UDP] [CONNECT] [USER " + user_ + "] [DST " +
                        target.address().to_string() + ":" + std::to_string(target.port()) + "]",
                Log::Level::INFO);

    slot = std::move(s);
    receive(*slot);
    return slot.get();
}

void UdpSession::receive(Socket &s) {
    s.socket.async_receive_from(
            boost::asio::buffer(s.buffer), s.sender,
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this(), &s](const boost::system::error_code &ec, std::size_t n) {
                        if (self->closed_ || !s.socket.is_open()) return;

                        if (ec) {
                            if (ec == boost::asio::error::connection_refused ||
                                ec == boost::asio::error::connection_reset) {
                                self->receive(s);
                            }
                            return;
                        }

                        std::string body = udp_relay::encodeSocksAddress(s.sender);
                        body.append(s.buffer.data(), n);
                        if (body.size() <= udp_relay::MaxDatagram) {
                            Usage::instance().add(self->usage_, n);
                            self->touch();
                            self->deliver(std::move(body));
                        }

                        self->receive(s);
                    }));
}

void UdpSession::deliver(std::string body) {
    if (sink_) {
        sink_(std::move(body));
        return;
    }

    std::lock_guard<std::mutex> lock(queueMutex_);
    if (queue_.size() >= MaxQueuedDatagrams || queuedBytes_ + body.size() > MaxQueuedBytes) return;
    queuedBytes_ += body.size();
    queue_.push_back(std::move(body));
}

std::string UdpSession::drain(std::size_t maxBytes) {
    touch();

    std::string out;
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!queue_.empty()) {
        const auto &front = queue_.front();
        if (!out.empty() && out.size() + udp_relay::FrameHeader + front.size() > maxBytes) break;
        udp_relay::appendFrame(out, front);
        queuedBytes_ -= front.size();
        queue_.pop_front();
    }
    return out;
}

void UdpSession::close() {
    if (closed_.exchange(true)) return;

    boost::asio::post(strand_, [self = shared_from_this()] {
        boost::system::error_code ignored;
        self->idleTimer_.cancel();
        if (self->v4_) self->v4_->socket.close(ignored);
        if (self->v6_) self->v6_->socket.close(ignored);
        self->sink_ = nullptr;

        std::lock_guard<std::mutex> lock(self->queueMutex_);
        self->queue_.clear();
        self->queuedBytes_ = 0;
    });
}

void UdpSession::sweepLocked() {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = registry_.begin(); it != registry_.end();) {
        if (it->second->closed() || it->second->idle(now)) {
            it->second->close();
            it = registry_.erase(it);
        } else {
            ++it;
        }
    }
}

bool UdpSession::add(const std::string &key, const pointer &session) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    sweepLocked();
    auto it = registry_.find(key);
    if (it != registry_.end()) {
        if (it->second->user() != session->user()) return false;
        it->second->close();
    }
    registry_[key] = session;
    return true;
}

UdpSession::pointer UdpSession::find(const std::string &key, const std::string &user) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = registry_.find(key);
    if (it == registry_.end() || it->second->user() != user || it->second->closed()) return nullptr;
    return it->second;
}

void UdpSession::remove(const std::string &key, const std::string &user) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = registry_.find(key);
    if (it == registry_.end() || it->second->user() != user) return;
    it->second->close();
    registry_.erase(it);
}

std::size_t UdpSession::revoke(const std::vector<std::string> &users) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    std::size_t closed = 0;
    for (auto it = registry_.begin(); it != registry_.end();) {
        if (std::find(users.begin(), users.end(), it->second->user()) != users.end()) {
            it->second->close();
            it = registry_.erase(it);
            ++closed;
        } else {
            ++it;
        }
    }
    return closed;
}

void UdpSession::closeAll() {
    std::unordered_map<std::string, pointer> sessions;
    {
        std::lock_guard<std::mutex> lock(registryMutex_);
        sessions.swap(registry_);
    }
    for (auto &[key, session]: sessions) session->close();
}


UdpAssociation::UdpAssociation(boost::asio::io_context &io_context,
                               const std::shared_ptr<Config> &config,
                               const std::shared_ptr<Log> &log,
                               TCPClient::pointer client,
                               boost::uuids::uuid uuid,
                               boost::asio::ip::address clientAddress,
                               boost::asio::ip::address bindAddress)
    : io_context_(io_context),
      config_(config),
      log_(log),
      client_(std::move(client)),
      uuid_(uuid),
      clientAddress_(clientAddress),
      bindAddress_(bindAddress),
      strand_(boost::asio::make_strand(io_context)),
      socket_(io_context),
      recvBuffer_(udp_relay::MaxDatagram + 1),
      pollTimer_(io_context) {}

std::string UdpAssociation::tag() const { return "[" + to_string(uuid_) + "] [UDP]"; }

UdpAssociation::udp::endpoint UdpAssociation::localEndpoint() const {
    boost::system::error_code ec;
    return socket_.local_endpoint(ec);
}

std::optional<std::string> UdpAssociation::post(const std::string &action,
                                                const std::string &rawBody,
                                                bool keepOpen) {
    std::lock_guard<std::mutex> lock(postMutex_);
    client_->uuid_ = uuid_;
    return postRelayAction(*client_, config_, log_, uuid_, action, rawBody, keepOpen);
}

bool UdpAssociation::open() {
    boost::system::error_code ec;
    socket_.open(bindAddress_.is_v4() ? udp::v4() : udp::v6(), ec);
    if (!ec) socket_.bind(udp::endpoint(bindAddress_, 0), ec);
    if (!ec) enlargeBuffers(socket_);
    if (ec) {
        log_->write(tag() + " [ERROR] [Bind] " + ec.message(), Log::Level::ERROR);
        return false;
    }

    tunnel_ = config_->general().tunnelEnable;

    if (!post("udpopen", "", tunnel_)) {
        log_->write(tag() + " [ERROR] [Server refused UDP ASSOCIATE] (server too old, or connection failed)",
                    Log::Level::INFO);
        socket_.close(ec);
        client_->socketShutdown();
        return false;
    }

    log_->write(tag() + " [ASSOCIATE] [SRC " + clientAddress_.to_string() + "] [BIND " +
                        localEndpoint().address().to_string() + ":" +
                        std::to_string(localEndpoint().port()) + "]",
                Log::Level::INFO);
    return true;
}

void UdpAssociation::start() {
    auto self = shared_from_this();
    boost::asio::post(strand_, [self] {
        self->receiveFromClient();
        if (self->tunnel_) {
            self->readFromServer();
        } else {
            self->pollDelayMs_ = self->config_->agent().pullTimeout;
            self->pollServer();
        }
    });
}

std::optional<std::string> UdpAssociation::acceptClientPacket(std::size_t bytes) {
    if (sender_.address() != clientAddress_) return std::nullopt;
    if (bytes > udp_relay::MaxDatagram) return std::nullopt;

    clientEndpoint_ = sender_;
    return udp_relay::socksPacketToBody(std::string(recvBuffer_.data(), bytes));
}

void UdpAssociation::receiveFromClient() {
    if (closed_) return;

    socket_.async_receive_from(
            boost::asio::buffer(recvBuffer_), sender_,
            boost::asio::bind_executor(
                    strand_,
                    [self = shared_from_this()](const boost::system::error_code &ec, std::size_t bytes) {
                        if (self->closed_) return;

                        if (ec) {
                            if (ec == boost::asio::error::connection_refused ||
                                ec == boost::asio::error::connection_reset) {
                                self->receiveFromClient();
                                return;
                            }
                            self->close();
                            return;
                        }

                        std::string batch;
                        if (auto body = self->acceptClientPacket(bytes)) {
                            udp_relay::appendFrame(batch, *body);
                        }

                        boost::system::error_code aec;
                        while (batch.size() < MaxSendBatch && self->socket_.available(aec) > 0 && !aec) {
                            const std::size_t n = self->socket_.receive_from(
                                    boost::asio::buffer(self->recvBuffer_), self->sender_, 0, aec);
                            if (aec) break;
                            if (auto body = self->acceptClientPacket(n)) {
                                udp_relay::appendFrame(batch, *body);
                            }
                        }

                        if (!batch.empty()) {
                            if (self->tunnel_) {
                                self->writeToServer(std::move(batch));
                            } else {
                                if (!self->post("udpsend", batch)) {
                                    self->log_->write(self->tag() + " [Closed] udpsend refused or failed",
                                                      Log::Level::INFO);
                                    self->close();
                                    return;
                                }
                                self->pollDelayMs_ = self->config_->agent().pullTimeout;
                                if (self->pollInProgress_) self->pollTimer_.cancel();
                            }
                        }

                        self->receiveFromClient();
                    }));
}

void UdpAssociation::sendToClient(const std::string &body) {
    if (!clientEndpoint_) return;

    const std::string packet = udp_relay::bodyToSocksPacket(body);
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(packet), *clientEndpoint_, 0, ec);
}

void UdpAssociation::pollServer() {
    if (closed_ || pollInProgress_) return;

    pollInProgress_ = true;
    pollTimer_.expires_after(std::chrono::milliseconds(pollDelayMs_));
    pollTimer_.async_wait(boost::asio::bind_executor(
            strand_, [self = shared_from_this()](const boost::system::error_code &ec) {
                self->pollInProgress_ = false;
                if (self->closed_) return;

                if (ec == boost::asio::error::operation_aborted) {
                    self->pollServer();
                    return;
                }
                if (ec) return;

                auto reply = self->post("udprecv", "");
                if (!reply) {
                    self->log_->write(self->tag() + " [Closed] udprecv refused or failed", Log::Level::INFO);
                    self->close();
                    return;
                }

                bool ok = true;
                const auto frames = udp_relay::takeFrames(*reply, ok);
                for (const auto &body: frames) self->sendToClient(body);

                const unsigned int baseDelay = self->config_->agent().pullTimeout;
                self->pollDelayMs_ =
                        frames.empty()
                                ? std::min(std::max(self->pollDelayMs_ * 2, 1u), std::max(MaxPollDelayMs, baseDelay))
                                : baseDelay;

                self->pollServer();
            }));
}

void UdpAssociation::readFromServer() {
    if (closed_) return;

    auto handler = boost::asio::bind_executor(
            strand_,
            [self = shared_from_this()](const boost::system::error_code &ec, std::size_t bytes) {
                if (self->closed_) return;
                if (ec || bytes == 0) {
                    self->close();
                    return;
                }

                self->serverPending_.append(self->serverBuffer_.data(), bytes);
                bool ok = true;
                const auto frames = udp_relay::takeFrames(self->serverPending_, ok);
                if (!ok) {
                    self->log_->write(self->tag() + " [Closed] malformed frame from server", Log::Level::INFO);
                    self->close();
                    return;
                }
                for (const auto &body: frames) self->sendToClient(body);

                self->readFromServer();
            });

    if (client_->tlsEnabled()) {
        client_->sslSocket().async_read_some(boost::asio::buffer(serverBuffer_), handler);
    } else {
        client_->socket().async_read_some(boost::asio::buffer(serverBuffer_), handler);
    }
}

void UdpAssociation::writeToServer(std::string frames) {
    if (writeQueuedBytes_ + frames.size() > MaxWriteQueueBytes) return;

    writeQueuedBytes_ += frames.size();
    writeQueue_.push_back(std::move(frames));
    if (writeQueue_.size() == 1) writeNext();
}

void UdpAssociation::writeNext() {
    if (closed_ || writeQueue_.empty()) return;

    auto handler = boost::asio::bind_executor(
            strand_, [self = shared_from_this()](const boost::system::error_code &ec, std::size_t) {
                if (self->closed_) return;
                if (ec) {
                    self->close();
                    return;
                }
                self->writeQueuedBytes_ -= self->writeQueue_.front().size();
                self->writeQueue_.pop_front();
                self->writeNext();
            });

    const auto buffer = boost::asio::buffer(writeQueue_.front());
    if (client_->tlsEnabled()) {
        boost::asio::async_write(client_->sslSocket(), buffer, handler);
    } else {
        boost::asio::async_write(client_->socket(), buffer, handler);
    }
}

void UdpAssociation::close() {
    if (closed_.exchange(true)) return;

    boost::asio::post(strand_, [self = shared_from_this()] {
        boost::system::error_code ignored;
        self->pollTimer_.cancel();
        self->socket_.close(ignored);

        if (!self->tunnel_) self->post("close", "");
        self->client_->socketShutdown();

        self->log_->write(self->tag() + " [Association closed]", Log::Level::DEBUG);
    });
}

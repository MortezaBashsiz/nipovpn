#include "serverhandler.hpp"

#include <sstream>
#include <utility>
#include <vector>

#include "http_utils.hpp"

std::mutex ServerHandler::sessionsMutex_;
std::unordered_map<std::string, ServerHandler::Session> ServerHandler::sessions_;

std::size_t ServerHandler::revokeSessions(const std::vector<std::string> &users) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    std::size_t closed = 0;
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        bool revoked = false;
        for (const auto &name: users) {
            if (it->second.user == name) {
                revoked = true;
                break;
            }
        }
        if (revoked) {
            it->second.client->socketShutdown();
            it = sessions_.erase(it);
            ++closed;
        } else {
            ++it;
        }
    }
    return closed;
}

std::string ServerHandler::HttpUtils::lowerCopy(std::string s) {
    return http_utils::toLowerCopy(std::move(s));
}

std::string ServerHandler::HttpUtils::trimCopy(std::string s) {
    return http_utils::trimCopy(std::move(s));
}

std::string ServerHandler::HttpUtils::getRawHeader(const std::string &headers,
                                                   const std::string &name) {
    return http_utils::getRawHeader(headers, name);
}

std::string ServerHandler::HttpUtils::extractHttpBody(const std::string &msg) {
    return http_utils::extractBody(msg);
}

ServerHandler::ServerHandler(boost::asio::streambuf &readBuffer,
                             boost::asio::streambuf &writeBuffer,
                             const std::shared_ptr<Config> &config,
                             const std::shared_ptr<Log> &log,
                             TCPClient::pointer client,
                             std::string clientConnStr,
                             boost::uuids::uuid uuid,
                             std::string &userHint)
    : config_(config),
      log_(log),
      client_(std::move(client)),
      readBuffer_(readBuffer),
      writeBuffer_(writeBuffer),
      request_(HTTP::create(config, log, readBuffer, uuid)),
      clientConnStr_(std::move(clientConnStr)),
      uuid_(uuid),
      userHint_(userHint) {
    end_ = false;
    connect_ = false;
}

BoolStr ServerHandler::authenticate(const std::string &payload) {
    const std::vector<User> &users = config_->users();

    if (!config_->general().aesEnable) {
        // Only possible without a users list: the single "default" user.
        user_ = users.empty() ? nullptr : &users.front();
        return {true, payload};
    }

    BoolStr result{false, "no enabled user's token authenticates the request"};
    auto tryUser = [&](const User &u) {
        if (!u.enabled) return false;
        BoolStr r = aes256DecryptWithKey(payload, u.key);
        if (!r.ok) return false;
        result = std::move(r);
        user_ = &u;
        userHint_ = u.name;
        return true;
    };

    // Most requests on a connection come from the same user.
    if (!userHint_.empty()) {
        for (const auto &u: users) {
            if (u.name == userHint_) {
                if (tryUser(u)) return result;
                break;
            }
        }
    }
    for (const auto &u: users) {
        if (u.name != userHint_ && tryUser(u)) return result;
    }
    return result;
}

std::string ServerHandler::srcTag() const {
    return clientConnStr_ + (user_ ? "] [USER " + user_->name : std::string());
}

ServerHandler::~ServerHandler() {}

void ServerHandler::makePlainHttpResponse(const std::string &body,
                                          const std::string &status,
                                          const std::string &connection) {
    std::ostringstream outer;

    outer << "HTTP/1.1 " << status << "\r\n"
          << "Content-Type: application/octet-stream\r\n"
          << "Content-Length: " << body.size() << "\r\n"
          << "Connection: " << connection << "\r\n"
          << "\r\n"
          << body;

    copyStringToStreambuf(outer.str(), writeBuffer_);

    end_ = false;
    connect_ = false;
}

void ServerHandler::makePayloadHttpResponse(const std::string &plainBody,
                                            const std::string &status,
                                            const std::string &connection) {
    BoolStr encryption{false, "no authenticated user to encrypt for"};
    if (!config_->general().aesEnable) {
        encryption = {true, plainBody};
    } else if (user_) {
        encryption = aes256EncryptWithKey(plainBody, user_->key);
    }

    if (!encryption.ok) {
        log_->write("[" + to_string(uuid_) + "] [ServerHandler] [Encryption Failed] " + encryption.message,
                    Log::Level::DEBUG);

        makePlainHttpResponse("", "500 Internal Server Error", "close");
        return;
    }

    makePlainHttpResponse(encryption.message, status, connection);
}

void ServerHandler::handle() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!request_->detectType()) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [ServerHandler handle] [NOT HTTP Request From Agent] [Request] : " +
                        streambufToString(readBuffer_),
                Log::Level::DEBUG);

        client_->socket().close();
        return;
    }

    const auto &outerReq = request_->parsedHttpRequest();

    const std::string outerRaw = streambufToString(readBuffer_);

    std::string requestBody = std::string(outerReq.body());

    if (requestBody.empty()) {
        requestBody = HttpUtils::extractHttpBody(outerRaw);
    }

    BoolStr decryption{false, std::string("FAILED")};

    try {
        decryption = authenticate(decode64(requestBody));
    } catch (const std::exception &e) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [ServerHandler handle] [Base64 Decode Failed] " + e.what(),
                Log::Level::DEBUG);

        makePlainHttpResponse("", "400 Bad Request", "close");
        return;
    }

    if (!decryption.ok) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [ServerHandler handle] [Decryption Failed] : [ " +
                        decryption.message + "] ",
                Log::Level::DEBUG);

        makePlainHttpResponse("", "400 Bad Request", "close");
        return;
    }

    Envelope envelope;
    if (!parseEnvelope(decryption.message, envelope)) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [ServerHandler handle] [Invalid Envelope] (agent and server versions or aesEnable may differ)",
                Log::Level::DEBUG);

        makePlainHttpResponse("", "400 Bad Request", "close");
        return;
    }

    const std::string &action = envelope.action;
    const std::string &session = envelope.session;
    decryption.message = std::move(envelope.data);

    log_->write(
            "[" + to_string(uuid_) + "] [ServerHandler handle] [Payload Decoded]",
            Log::Level::DEBUG);

    if (action == "send") {
        TCPClient::pointer targetClient;

        {
            std::lock_guard<std::mutex> sessionLock(sessionsMutex_);
            auto it = sessions_.find(session);

            if (it != sessions_.end() && it->second.user == userName()) {
                targetClient = it->second.client;
            }
        }

        if (!targetClient || !targetClient->isOpen()) {
            makePayloadHttpResponse("", "200 OK", (config_->general().connectionReuse ? "keep-alive" : "close"));
            return;
        }

        boost::asio::streambuf tunnelBuf;
        copyStringToStreambuf(hexToASCII(decryption.message), tunnelBuf);
        targetClient->doWrite(tunnelBuf);

        makePayloadHttpResponse("", "200 OK", (config_->general().connectionReuse ? "keep-alive" : "close"));
        return;
    }

    if (action == "recv") {
        TCPClient::pointer targetClient;

        {
            std::lock_guard<std::mutex> sessionLock(sessionsMutex_);
            auto it = sessions_.find(session);

            if (it != sessions_.end() && it->second.user == userName()) {
                targetClient = it->second.client;
            }
        }

        if (!targetClient || !targetClient->isOpen()) {
            makePayloadHttpResponse("", "200 OK", (config_->general().connectionReuse ? "keep-alive" : "close"));
            return;
        }

        boost::system::error_code ec;
        std::size_t available = targetClient->socket().available(ec);

        if (ec || available == 0) {
            makePayloadHttpResponse("", "200 OK", (config_->general().connectionReuse ? "keep-alive" : "close"));
            return;
        }

        std::vector<char> data(std::min<std::size_t>(available, config_->general().bufferSize));
        std::size_t n = targetClient->socket().read_some(
                boost::asio::buffer(data),
                ec);

        if (ec || n == 0) {
            makePayloadHttpResponse("", "200 OK", (config_->general().connectionReuse ? "keep-alive" : "close"));
            return;
        }

        makePayloadHttpResponse(
                std::string(data.data(), n),
                "200 OK",
                (config_->general().connectionReuse ? "keep-alive" : "close"));

        return;
    }

    if (action == "close") {
        std::lock_guard<std::mutex> sessionLock(sessionsMutex_);
        auto it = sessions_.find(session);

        if (it != sessions_.end() && it->second.user == userName()) {
            it->second.client->socketShutdown();
            sessions_.erase(it);
        }

        makePayloadHttpResponse("", "200 OK", (config_->general().connectionReuse ? "keep-alive" : "close"));
        return;
    }

    const std::string innerRequest = hexToASCII(decryption.message);

    const std::size_t headerEnd = innerRequest.find("\r\n\r\n");

    if (headerEnd == std::string::npos) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [ServerHandler handle] [INNER REQUEST MISSING HEADER END]",
                Log::Level::DEBUG);

        makePayloadHttpResponse("", "400 Bad Request", "close");
        return;
    }

    const std::string innerHeaders = innerRequest.substr(0, headerEnd + 4);
    const std::string pendingTunnelData = innerRequest.substr(headerEnd + 4);

    copyStringToStreambuf(innerHeaders, readBuffer_);

    HTTP::pointer inner = HTTP::create(config_, log_, readBuffer_, uuid_);

    if (!inner->detectType()) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [ServerHandler handle] [NOT INNER HTTP Request] [Request] : " +
                        streambufToString(readBuffer_),
                Log::Level::DEBUG);

        makePayloadHttpResponse("", "400 Bad Request", "close");
        return;
    }

    switch (inner->httpType()) {
        case HTTP::HttpType::connect: {
            if (config_->general().tunnelEnable) {
                if (!client_->doConnect(inner->dstIp(), inner->dstPort())) {
                    log_->write(
                            "[" + to_string(uuid_) +
                                    "] [CONNECT] [ERROR] [Resolving Host] [SRC " +
                                    srcTag() + "] [DST " +
                                    inner->dstIp() + ":" + std::to_string(inner->dstPort()) + "]",
                            Log::Level::INFO);

                    makePayloadHttpResponse(
                            "HTTP/1.1 502 Bad Gateway\r\n\r\n",
                            "502 Bad Gateway",
                            "close");

                    connect_ = false;
                    end_ = false;
                    return;
                }

                log_->write(
                        "[" + to_string(uuid_) +
                                "] [CONNECT] [SRC " +
                                srcTag() + "] [DST " +
                                inner->dstIp() + ":" + std::to_string(inner->dstPort()) + "]",
                        Log::Level::INFO);

                makePayloadHttpResponse(
                        "HTTP/1.1 200 Connection Established\r\n\r\n",
                        "200 OK",
                        "keep-alive");

                connect_ = true;
                end_ = false;
                return;
            }

            TCPClient::pointer targetClient = TCPClient::create(
                    static_cast<boost::asio::io_context &>(
                            client_->socket().get_executor().context()),
                    config_,
                    log_);

            targetClient->uuid_ = uuid_;

            if (!targetClient->doConnect(inner->dstIp(), inner->dstPort())) {
                log_->write(
                        "[" + to_string(uuid_) +
                                "] [CONNECT] [ERROR] [Resolving Host] [SRC " +
                                srcTag() + "] [DST " +
                                inner->dstIp() + ":" + std::to_string(inner->dstPort()) + "]",
                        Log::Level::INFO);

                makePayloadHttpResponse(
                        "HTTP/1.1 502 Bad Gateway\r\n\r\n",
                        "502 Bad Gateway",
                        "close");

                connect_ = false;
                end_ = false;
                return;
            }

            log_->write(
                    "[" + to_string(uuid_) +
                            "] [CONNECT] [SRC " +
                            srcTag() + "] [ORIGIN " +
                            inner->dstIp() + ":" + std::to_string(inner->dstPort()) + "]",
                    Log::Level::INFO);

            if (!pendingTunnelData.empty()) {
                boost::asio::streambuf pendingBuf;
                copyStringToStreambuf(pendingTunnelData, pendingBuf);
                targetClient->doWrite(pendingBuf);
            }

            {
                std::lock_guard<std::mutex> sessionLock(sessionsMutex_);
                const std::string key = session.empty() ? to_string(uuid_) : session;
                auto it = sessions_.find(key);

                // Never let one user take over another user's session.
                if (it != sessions_.end() && it->second.user != userName()) {
                    targetClient->socketShutdown();
                    makePayloadHttpResponse("", "403 Forbidden", "close");
                    return;
                }

                sessions_[key] = Session{targetClient, userName()};
            }

            makePayloadHttpResponse(
                    "HTTP/1.1 200 Connection Established\r\n\r\n",
                    "200 OK",
                    "keep-alive");

            connect_ = false;
            end_ = false;
            return;
        }

        case HTTP::HttpType::http:
        case HTTP::HttpType::https: {
            copyStringToStreambuf(innerRequest, readBuffer_);

            if (!client_->socket().is_open()) {
                if (!client_->doConnect(inner->dstIp(), inner->dstPort())) {
                    client_->socket().close();
                    makePayloadHttpResponse("", "502 Bad Gateway", "close");
                    return;
                }
            }

            client_->doWrite(readBuffer_);
            client_->doReadServer();

            if (client_->readBuffer().size() == 0) {
                client_->socket().close();
                makePayloadHttpResponse("", "200 OK", (config_->general().connectionReuse ? "keep-alive" : "close"));
                return;
            }

            makePayloadHttpResponse(
                    streambufToString(client_->readBuffer()),
                    "200 OK",
                    (config_->general().connectionReuse ? "keep-alive" : "close"));

            connect_ = false;
            end_ = false;
            return;
        }
    }
}
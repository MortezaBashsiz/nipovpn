#include "serverhandler.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

std::mutex ServerHandler::sessionsMutex_;
std::unordered_map<std::string, TCPClient::pointer> ServerHandler::sessions_;

std::string ServerHandler::HttpUtils::lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return s;
}

std::string ServerHandler::HttpUtils::trimCopy(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.erase(s.begin());
    }

    while (!s.empty() &&
           (s.back() == ' ' ||
            s.back() == '\t' ||
            s.back() == '\r' ||
            s.back() == '\n')) {
        s.pop_back();
    }

    return s;
}

std::string ServerHandler::HttpUtils::getRawHeader(const std::string &headers,
                                                   const std::string &name) {
    std::istringstream iss(headers);
    std::string line;

    const std::string prefix = lowerCopy(name) + ":";

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string lower = lowerCopy(line);

        if (lower.rfind(prefix, 0) == 0) {
            return trimCopy(line.substr(name.size() + 1));
        }
    }

    return "";
}

std::string ServerHandler::HttpUtils::extractHttpBody(const std::string &msg) {
    const auto pos = msg.find("\r\n\r\n");

    if (pos == std::string::npos) {
        return "";
    }

    return msg.substr(pos + 4);
}

ServerHandler::ServerHandler(boost::asio::streambuf &readBuffer,
                             boost::asio::streambuf &writeBuffer,
                             const std::shared_ptr<Config> &config,
                             const std::shared_ptr<Log> &log,
                             const TCPClient::pointer &client,
                             const std::string &clientConnStr,
                             boost::uuids::uuid uuid)
    : config_(config),
      log_(log),
      client_(client),
      readBuffer_(readBuffer),
      writeBuffer_(writeBuffer),
      request_(HTTP::create(config, log, readBuffer, uuid)),
      clientConnStr_(clientConnStr),
      uuid_(uuid) {
    end_ = false;
    connect_ = false;
}

ServerHandler::~ServerHandler() {}

void ServerHandler::makePlainHttpResponse(const std::string &body,
                                          const std::string &status,
                                          const std::string &connection) {
    std::ostringstream outer;

    outer << "HTTP/" << config_->agent().httpVersion << status << "\r\n"
          << "Content-Type: application/octet-stream\r\n"
          << "Content-Length: " << body.size() << "\r\n"
          << "Connection: " << connection << "\r\n"
          << "\r\n"
          << body;

    copyStringToStreambuf(outer.str(), writeBuffer_);

    end_ = false;
    connect_ = false;
}

void ServerHandler::makeEncryptedHttpResponse(const std::string &plainBody,
                                              const std::string &status,
                                              const std::string &connection) {
    BoolStr encryption = aes256Encrypt(plainBody, config_->general().token);

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
    const auto outerHeaderEnd = outerRaw.find("\r\n\r\n");
    const std::string outerHeaders =
            outerHeaderEnd == std::string::npos
                    ? ""
                    : outerRaw.substr(0, outerHeaderEnd + 2);

    const std::string session =
            HttpUtils::getRawHeader(outerHeaders, "X-Nipo-Session");

    const std::string action =
            HttpUtils::getRawHeader(outerHeaders, "X-Nipo-Action");

    std::string requestBody = std::string(outerReq.body());

    if (requestBody.empty()) {
        requestBody = HttpUtils::extractHttpBody(outerRaw);
    }

    BoolStr decryption{false, std::string("FAILED")};

    try {
        decryption = aes256Decrypt(
                decode64(requestBody),
                config_->general().token);
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

    log_->write(
            "[" + to_string(uuid_) + "] [ServerHandler handle] [Decryption Done]",
            Log::Level::DEBUG);

    if (action == "send") {
        TCPClient::pointer targetClient;

        {
            std::lock_guard<std::mutex> sessionLock(sessionsMutex_);
            auto it = sessions_.find(session);

            if (it != sessions_.end()) {
                targetClient = it->second;
            }
        }

        if (!targetClient || !targetClient->isOpen()) {
            makeEncryptedHttpResponse("", "200 OK", "close");
            return;
        }

        boost::asio::streambuf tunnelBuf;
        copyStringToStreambuf(hexToASCII(decryption.message), tunnelBuf);
        targetClient->doWrite(tunnelBuf);

        makeEncryptedHttpResponse("", "200 OK", "close");
        return;
    }

    if (action == "recv") {
        TCPClient::pointer targetClient;

        {
            std::lock_guard<std::mutex> sessionLock(sessionsMutex_);
            auto it = sessions_.find(session);

            if (it != sessions_.end()) {
                targetClient = it->second;
            }
        }

        if (!targetClient || !targetClient->isOpen()) {
            makeEncryptedHttpResponse("", "200 OK", "close");
            return;
        }

        boost::system::error_code ec;
        std::size_t available = targetClient->socket().available(ec);

        if (ec || available == 0) {
            makeEncryptedHttpResponse("", "200 OK", "close");
            return;
        }

        std::vector<char> data(std::min<std::size_t>(available, 65536));
        std::size_t n = targetClient->socket().read_some(
                boost::asio::buffer(data),
                ec);

        if (ec || n == 0) {
            makeEncryptedHttpResponse("", "200 OK", "close");
            return;
        }

        makeEncryptedHttpResponse(
                std::string(data.data(), n),
                "200 OK",
                "close");

        return;
    }

    if (action == "close") {
        std::lock_guard<std::mutex> sessionLock(sessionsMutex_);
        auto it = sessions_.find(session);

        if (it != sessions_.end()) {
            it->second->socketShutdown();
            sessions_.erase(it);
        }

        makeEncryptedHttpResponse("", "200 OK", "close");
        return;
    }

    const std::string innerRequest = hexToASCII(decryption.message);

    const std::size_t headerEnd = innerRequest.find("\r\n\r\n");

    if (headerEnd == std::string::npos) {
        log_->write(
                "[" + to_string(uuid_) +
                        "] [ServerHandler handle] [INNER REQUEST MISSING HEADER END]",
                Log::Level::DEBUG);

        makeEncryptedHttpResponse("", "400 Bad Request", "close");
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

        makeEncryptedHttpResponse("", "400 Bad Request", "close");
        return;
    }

    switch (inner->httpType()) {
        case HTTP::HttpType::connect: {
            if (config_->general().tunnelEnable) {
                if (!client_->doConnect(inner->dstIP(), inner->dstPort())) {
                    log_->write(
                            "[" + to_string(uuid_) +
                                    "] [CONNECT] [ERROR] [Resolving Host] [SRC " +
                                    clientConnStr_ + "] [DST " +
                                    inner->dstIP() + ":" + std::to_string(inner->dstPort()) + "]",
                            Log::Level::INFO);

                    makeEncryptedHttpResponse(
                            "HTTP/" + config_->agent().httpVersion + "502 Bad Gateway\r\n\r\n",
                            "502 Bad Gateway",
                            "close");

                    connect_ = false;
                    end_ = false;
                    return;
                }

                log_->write(
                        "[" + to_string(uuid_) +
                                "] [CONNECT] [TUNNEL] [SRC " +
                                clientConnStr_ + "] [DST " +
                                inner->dstIP() + ":" + std::to_string(inner->dstPort()) + "]",
                        Log::Level::INFO);

                makeEncryptedHttpResponse(
                        "HTTP/" + config_->agent().httpVersion + " 200 Connection Established\r\n\r\n",
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

            if (!targetClient->doConnect(inner->dstIP(), inner->dstPort())) {
                log_->write(
                        "[" + to_string(uuid_) +
                                "] [CONNECT] [ERROR] [Resolving Host] [SRC " +
                                clientConnStr_ + "] [DST " +
                                inner->dstIP() + ":" + std::to_string(inner->dstPort()) + "]",
                        Log::Level::INFO);

                makeEncryptedHttpResponse(
                        "HTTP/" + config_->agent().httpVersion + " 502 Bad Gateway\r\n\r\n",
                        "502 Bad Gateway",
                        "close");

                connect_ = false;
                end_ = false;
                return;
            }

            log_->write(
                    "[" + to_string(uuid_) +
                            "] [CONNECT] [SRC " +
                            clientConnStr_ + "] [DST " +
                            inner->dstIP() + ":" + std::to_string(inner->dstPort()) + "]",
                    Log::Level::INFO);

            if (!pendingTunnelData.empty()) {
                boost::asio::streambuf pendingBuf;
                copyStringToStreambuf(pendingTunnelData, pendingBuf);
                targetClient->doWrite(pendingBuf);
            }

            {
                std::lock_guard<std::mutex> sessionLock(sessionsMutex_);
                sessions_[session.empty() ? to_string(uuid_) : session] = targetClient;
            }

            makeEncryptedHttpResponse(
                    "HTTP/" + config_->agent().httpVersion + " 200 Connection Established\r\n\r\n",
                    "200 OK",
                    "close");

            connect_ = false;
            end_ = false;
            return;
        }

        case HTTP::HttpType::http:
        case HTTP::HttpType::https: {
            copyStringToStreambuf(innerRequest, readBuffer_);

            if (!client_->socket().is_open()) {
                if (!client_->doConnect(inner->dstIP(), inner->dstPort())) {
                    client_->socket().close();
                    makeEncryptedHttpResponse("", "502 Bad Gateway", "close");
                    return;
                }
            }

            client_->doWrite(readBuffer_);
            client_->doReadServer();

            if (client_->readBuffer().size() == 0) {
                client_->socket().close();
                makeEncryptedHttpResponse("", "200 OK", "close");
                return;
            }

            makeEncryptedHttpResponse(
                    streambufToString(client_->readBuffer()),
                    "200 OK",
                    "close");

            connect_ = false;
            end_ = false;
            return;
        }
    }
}
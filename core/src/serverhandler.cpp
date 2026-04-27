#include "serverhandler.hpp"

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

void ServerHandler::handle() {
    std::lock_guard lock(mutex_);

    if (!request_->detectType()) {
        log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [NOT HTTP Request From Agent] [Request] : " +
                            streambufToString(readBuffer_),
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    const auto &outerReq = request_->parsedHttpRequest();

    const std::string method = std::string(outerReq.method_string());
    const std::string target = std::string(outerReq.target());

    if (method != "POST" || target != "/relay") {
        copyStringToStreambuf(
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n",
                writeBuffer_);
        return;
    }

    const std::string encodedOuterBody = std::string(outerReq.body());
    const std::string encryptedOuterBody = decode64(encodedOuterBody);

    BoolStr decryption = aes256Decrypt(encryptedOuterBody, config_->general().token);
    if (!decryption.ok) {
        log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Decryption Failed] : [ " +
                            decryption.message + "] ",
                    Log::Level::DEBUG);

        const std::string plainError =
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n";

        BoolStr encryptedError = aes256Encrypt(plainError, config_->general().token);
        const std::string encodedError = encode64(encryptedError.message);

        std::ostringstream outer;
        outer << "HTTP/1.1 400 Bad Request\r\n"
              << "Content-Type: application/octet-stream\r\n"
              << "Content-Length: " << encodedError.size() << "\r\n"
              << "Connection: close\r\n"
              << "\r\n"
              << encodedError;

        copyStringToStreambuf(outer.str(), writeBuffer_);
        return;
    }

    const std::string innerRequest = hexToASCII(decryption.message);
    const std::size_t headerEnd = innerRequest.find("\r\n\r\n");

    if (headerEnd == std::string::npos) {
        log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [INNER REQUEST MISSING HEADER END]",
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    const std::string innerHeaders = innerRequest.substr(0, headerEnd + 4);
    const std::string pendingTunnelData = innerRequest.substr(headerEnd + 4);

    boost::asio::streambuf innerHeaderBuffer;
    copyStringToStreambuf(innerHeaders, innerHeaderBuffer);

    HTTP::pointer inner = HTTP::create(config_, log_, innerHeaderBuffer, uuid_);
    if (!inner->detectType()) {
        log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [NOT INNER HTTP Request] [Request] : " +
                            innerHeaders,
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    switch (inner->httpType()) {
        case HTTP::HttpType::connect: {
            if (!client_->doConnect(inner->dstIP(), inner->dstPort())) {
                log_->write("[" + to_string(uuid_) + "] [CONNECT] [ERROR] [Resolving Host] [SRC " +
                                    clientConnStr_ + "] [DST " + inner->dstIP() + ":" +
                                    std::to_string(inner->dstPort()) + "]",
                            Log::Level::INFO);

                const std::string connectFailed =
                        "HTTP/1.1 502 Bad Gateway\r\n"
                        "Content-Length: 0\r\n"
                        "\r\n";

                BoolStr encryption = aes256Encrypt(connectFailed, config_->general().token);
                const std::string encodedResponse = encode64(encryption.message);

                std::ostringstream outer;
                outer << "HTTP/1.1 502 Bad Gateway\r\n"
                      << "Content-Type: application/octet-stream\r\n"
                      << "Content-Length: " << encodedResponse.size() << "\r\n"
                      << "Connection: close\r\n"
                      << "\r\n"
                      << encodedResponse;

                copyStringToStreambuf(outer.str(), writeBuffer_);

                connect_ = false;
                end_ = true;
                return;
            }

            log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " + clientConnStr_ + "] [DST " +
                                inner->dstIP() + ":" + std::to_string(inner->dstPort()) + "]",
                        Log::Level::INFO);

            if (!pendingTunnelData.empty()) {
                boost::asio::streambuf pendingBuf;
                copyStringToStreambuf(pendingTunnelData, pendingBuf);
                client_->doWrite(pendingBuf);
            }

            const std::string connectEstablished =
                    "HTTP/1.1 200 Connection Established\r\n"
                    "\r\n";

            BoolStr encryption = aes256Encrypt(connectEstablished, config_->general().token);
            if (!encryption.ok) {
                log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Failed] : [ " +
                                    encryption.message + "] ",
                            Log::Level::DEBUG);
                client_->socket().close();
                return;
            }

            const std::string encodedResponse = encode64(encryption.message);

            std::ostringstream outer;
            outer << "HTTP/1.1 200 OK\r\n"
                  << "Content-Type: application/octet-stream\r\n"
                  << "Content-Length: " << encodedResponse.size() << "\r\n"
                  << "Connection: keep-alive\r\n"
                  << "\r\n"
                  << encodedResponse;

            copyStringToStreambuf(outer.str(), writeBuffer_);

            connect_ = true;
            end_ = false;
            return;
        }

        case HTTP::HttpType::http:
        case HTTP::HttpType::https: {
            boost::asio::streambuf upstreamRequest;
            copyStringToStreambuf(innerRequest, upstreamRequest);

            if (!client_->socket().is_open()) {
                if (!client_->doConnect(inner->dstIP(), inner->dstPort())) {
                    client_->socket().close();
                    return;
                }
            }

            client_->doWrite(upstreamRequest);
            client_->doReadServer();

            if (client_->readBuffer().size() == 0) {
                client_->socket().close();
                return;
            }

            const std::string upstreamResponse = streambufToString(client_->readBuffer());

            BoolStr encryption = aes256Encrypt(upstreamResponse, config_->general().token);
            if (!encryption.ok) {
                log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Failed] : [ " +
                                    encryption.message + "] ",
                            Log::Level::DEBUG);
                client_->socket().close();
                return;
            }

            const std::string encodedResponse = encode64(encryption.message);

            std::ostringstream outer;
            outer << "HTTP/1.1 200 OK\r\n"
                  << "Content-Type: application/octet-stream\r\n"
                  << "Content-Length: " << encodedResponse.size() << "\r\n"
                  << "Connection: keep-alive\r\n"
                  << "\r\n"
                  << encodedResponse;

            copyStringToStreambuf(outer.str(), writeBuffer_);

            connect_ = false;
            end_ = false;
            return;
        }
    }
}
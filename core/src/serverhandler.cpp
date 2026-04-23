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
        log_->write("[" + to_string(uuid_) +
                            "] [ServerHandler handle] [NOT HTTP Request From Agent] "
                            "[Request] : " +
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
                "Connection: close\r\n\r\n",
                writeBuffer_);
        return;
    }


    BoolStr decryption{false, std::string("FAILED")};
    decryption = aes256Decrypt(decode64(std::string(outerReq.body())), config_->general().token);

    if (decryption.ok) {
        copyStringToStreambuf(decryption.message, writeBuffer_);
        log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Decryption Done]", Log::Level::DEBUG);
    } else {
        log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Decryption Failed] : [ " +
                            decryption.message + "] ",
                    Log::Level::DEBUG);
    }

    const std::string innerRequest = hexToASCII(decryption.message);
    const std::size_t headerEnd = innerRequest.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        log_->write("[" + to_string(uuid_) +
                            "] [ServerHandler handle] [INNER REQUEST MISSING HEADER END]",
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    const std::string innerHeaders = innerRequest.substr(0, headerEnd + 4);
    const std::string pendingTunnelData = innerRequest.substr(headerEnd + 4);

    copyStringToStreambuf(innerHeaders, readBuffer_);
    HTTP::pointer inner = HTTP::create(config_, log_, readBuffer_, uuid_);

    if (!inner->detectType()) {
        log_->write("[" + to_string(uuid_) +
                            "] [ServerHandler handle] [NOT INNER HTTP Request] [Request] : " +
                            streambufToString(readBuffer_),
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    switch (inner->httpType()) {
        case HTTP::HttpType::connect: {
            if (client_->doConnect(inner->dstIP(), inner->dstPort())) {
                log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " + clientConnStr_ +
                                    "] [DST " + inner->dstIP() + ":" +
                                    std::to_string(inner->dstPort()) + "]",
                            Log::Level::INFO);

                if (!pendingTunnelData.empty()) {
                    boost::asio::streambuf pendingBuf;
                    copyStringToStreambuf(pendingTunnelData, pendingBuf);
                    client_->doWrite(pendingBuf);
                }

                const std::string connectEstablished =
                        "HTTP/1.1 200 Connection Established\r\n\r\n";

                BoolStr encryption{false, std::string("FAILED")};
                encryption = aes256Encrypt(connectEstablished, config_->general().token);

                if (encryption.ok) {
                    log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Done]", Log::Level::DEBUG);
                } else {
                    log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Failed] : [ " +
                                        encryption.message + "] ",
                                Log::Level::DEBUG);
                }

                std::ostringstream outer;
                outer << "HTTP/1.1 200 OK\r\n"
                      << "Content-Type: application/octet-stream\r\n"
                      << "Content-Length: " << encryption.message.size() << "\r\n"
                      << "Connection: keep-alive\r\n"
                      << "\r\n"
                      << encryption.message;

                copyStringToStreambuf(outer.str(), writeBuffer_);
                connect_ = true;
                end_ = false;
                return;
            } else {
                log_->write("[" + to_string(uuid_) +
                                    "] [CONNECT] [ERROR] [Resolving Host] [SRC " +
                                    clientConnStr_ + "] [DST " + inner->dstIP() + ":" +
                                    std::to_string(inner->dstPort()) + "]",
                            Log::Level::INFO);

                const std::string connectFailed =
                        "HTTP/1.1 502 Bad Gateway\r\n\r\n";

                BoolStr encryption{false, std::string("FAILED")};
                encryption = aes256Encrypt(connectFailed, config_->general().token);

                if (encryption.ok) {
                    log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Done]", Log::Level::DEBUG);
                } else {
                    log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Failed] : [ " +
                                        encryption.message + "] ",
                                Log::Level::DEBUG);
                }

                std::ostringstream outer;
                outer << "HTTP/1.1 502 Bad Gateway\r\n"
                      << "Content-Type: application/octet-stream\r\n"
                      << "Content-Length: " << encryption.message.size() << "\r\n"
                      << "Connection: close\r\n"
                      << "\r\n"
                      << encryption.message;

                copyStringToStreambuf(outer.str(), writeBuffer_);
                connect_ = false;
                end_ = true;
                return;
            }
        }

        case HTTP::HttpType::http:
        case HTTP::HttpType::https: {

            copyStringToStreambuf(innerRequest, readBuffer_);

            if (!client_->socket().is_open()) {
                if (!client_->doConnect(inner->dstIP(), inner->dstPort())) {
                    client_->socket().close();
                    return;
                }
            }

            client_->doWrite(readBuffer_);
            client_->doReadServer();

            if (client_->readBuffer().size() == 0) {
                client_->socket().close();
                return;
            }

            BoolStr encryption{false, std::string("FAILED")};
            encryption = aes256Encrypt(streambufToString(client_->readBuffer()), config_->general().token);

            if (encryption.ok) {
                log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Done]", Log::Level::DEBUG);
            } else {
                log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Failed] : [ " +
                                    encryption.message + "] ",
                            Log::Level::DEBUG);
            }
            const std::string innerResponse = encryption.message;

            std::ostringstream outer;
            outer << "HTTP/1.1 200 OK\r\n"
                  << "Content-Type: application/octet-stream\r\n"
                  << "Content-Length: " << innerResponse.size() << "\r\n"
                  << "Connection: keep-alive\r\n"
                  << "\r\n"
                  << innerResponse;

            copyStringToStreambuf(outer.str(), writeBuffer_);
            connect_ = false;
            end_ = false;
            return;
        }
    }
}
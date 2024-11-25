#include "serverhandler.hpp"

/**
 * @brief Constructs a `ServerHandler` object.
 * 
 * Initializes the HTTP request handler, client connection, and assigns references
 * to configuration, logging, and buffers.
 */
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

/**
 * @brief Destructor for `ServerHandler`.
 * 
 * Ensures cleanup of resources, though currently the destructor does not perform specific actions.
 */
ServerHandler::~ServerHandler() {}

/**
 * @brief Handles the incoming request and processes it based on its type.
 * 
 * Processes HTTP or HTTPS requests, decrypts payloads, performs appropriate actions
 * (e.g., connect or forward requests), and generates responses.
 */
void ServerHandler::handle() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (request_->detectType()) {
        log_->write(
                "[" + to_string(uuid_) + "] [ServerHandler handle] [Request From Agent] : " + request_->toString(),
                Log::Level::DEBUG);
        BoolStr decryption{false, std::string("FAILED")};
        decryption = aes256Decrypt(decode64(boost::lexical_cast<std::string>(
                                           request_->parsedHttpRequest().body())),
                                   config_->general().token);

        if (decryption.ok) {
            log_->write(
                    "[" + to_string(uuid_) + "] [ServerHandler handle] [Token Valid] : " + request_->toString(),
                    Log::Level::DEBUG);
            auto tempHexArr = strToHexArr(decryption.message);
            std::string tempHexArrStr(tempHexArr.begin(), tempHexArr.end());
            copyStringToStreambuf(tempHexArrStr, readBuffer_);

            if (request_->detectType()) {
                log_->write(
                        "[" + to_string(uuid_) + "] [ServerHandler handle] [Request] : " + request_->toString(),
                        Log::Level::DEBUG);
                switch (request_->httpType()) {
                    case HTTP::HttpType::connect: {
                        connect_ = true;
                        boost::asio::streambuf tempBuff;
                        std::iostream os(&tempBuff);
                        if (client_->doConnect(request_->dstIP(), request_->dstPort())) {
                            log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " + clientConnStr_ + "] [DST " +
                                                request_->dstIP() + ":" +
                                                std::to_string(request_->dstPort()) + "]",
                                        Log::Level::INFO);
                        } else {
                            log_->write(
                                    std::string("[" + to_string(uuid_) + "] [CONNECT] [ERROR] [Resolving Host] [SRC ") +
                                            clientConnStr_ + "] [DST " + request_->dstIP() + ":" +
                                            std::to_string(request_->dstPort()) + "]",
                                    Log::Level::INFO);
                        }
                        if (client_->socket().is_open()) {
                            std::string message(
                                    "HTTP/1.1 200 Connection established COMP\r\n\r\n");
                            os << message;
                        } else {
                            std::string message("HTTP/1.1 500 Connection failed COMP\r\n\r\n");
                            os << message;
                        }
                        moveStreambuf(tempBuff, writeBuffer_);
                    } break;
                    case HTTP::HttpType::http:
                    case HTTP::HttpType::https: {
                        if (request_->httpType() == HTTP::HttpType::http) {
                            if (client_->doConnect(request_->dstIP(), request_->dstPort())) {
                                log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " + clientConnStr_ + "] [DST " +
                                                    request_->dstIP() + ":" +
                                                    std::to_string(request_->dstPort()) + "]",
                                            Log::Level::INFO);
                            } else {
                                log_->write(
                                        std::string("[" + to_string(uuid_) + "] [CONNECT] [ERROR] [Resolving Host] [SRC ") +
                                                clientConnStr_ + "] [DST " + request_->dstIP() + ":" +
                                                std::to_string(request_->dstPort()) + "]",
                                        Log::Level::INFO);
                            }
                        }
                        if (!client_->socket().is_open()) {
                            client_->doConnect(request_->dstIP(), request_->dstPort());
                        }
                        client_->doWrite(readBuffer_);
                        client_->doHandle();
                        end_ = client_->end_;
                        if (client_->readBuffer().size() > 0) {
                            BoolStr encryption{false, std::string("FAILED")};
                            encryption =
                                    aes256Encrypt(streambufToString(client_->readBuffer()),
                                                  config_->general().token);
                            if (encryption.ok) {
                                if (end_) {
                                    request_->chunkHeader_ = "yes";
                                }
                                std::string newRes(
                                        request_->genHttpOkResString(encode64(encryption.message)));
                                copyStringToStreambuf(newRes, writeBuffer_);
                                if (request_->httpType() == HTTP::HttpType::http) {
                                    client_->socket().close();
                                } else {
                                    if (request_->parsedTlsRequest().type ==
                                        HTTP::TlsTypes::ApplicationData) {
                                        client_->socket().close();
                                    }
                                }
                            } else {
                                log_->write(
                                        "[" + to_string(uuid_) +
                                                "] [ServerHandler handle] [Encryption "
                                                "Failed] : [ " +
                                                encryption.message + "] ",
                                        Log::Level::DEBUG);
                                log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Failed] : " +
                                                    request_->toString(),
                                            Log::Level::INFO);
                                client_->socket().close();
                            }
                        } else {
                            client_->socket().close();
                            return;
                        }
                    } break;
                }
            } else {
                log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [NOT HTTP Request] [Request] : " +
                                    streambufToString(readBuffer_),
                            Log::Level::DEBUG);

                client_->socket().close();
                return;
            }
        } else {
            log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Decryption Failed] : [ " +
                                decryption.message + "] ",
                        Log::Level::DEBUG);
            log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Decryption Failed] : " +
                                request_->toString(),
                        Log::Level::INFO);

            client_->socket().close();
            return;
        }
    } else {
        log_->write(
                "[" + to_string(uuid_) + "] [ServerHandler handle] [NOT HTTP Request From Agent] [Request] : " +
                        streambufToString(readBuffer_),
                Log::Level::DEBUG);

        client_->socket().close();
        return;
    }
}

/**
 * @brief Continues reading and processing data from the client.
 * 
 * Encrypts responses and manages the client socket's lifecycle based on the request type.
 */
void ServerHandler::continueRead() {
    std::lock_guard<std::mutex> lock(mutex_);
    client_->doHandle();
    end_ = client_->end_;
    if (client_->readBuffer().size() > 0) {
        BoolStr encryption{false, std::string("FAILED")};
        encryption =
                aes256Encrypt(streambufToString(client_->readBuffer()),
                              config_->general().token);
        if (encryption.ok) {
            if (end_) {
                request_->chunkHeader_ = "yes";
            }
            std::string newRes(
                    request_->genHttpOkResString(encode64(encryption.message)));
            copyStringToStreambuf(newRes, writeBuffer_);
            if (request_->httpType() == HTTP::HttpType::http) {
                client_->socket().close();
            } else {
                if (request_->parsedTlsRequest().type ==
                    HTTP::TlsTypes::ApplicationData) {
                    client_->socket().close();
                }
            }
        } else {
            log_->write(
                    "[" + to_string(uuid_) +
                            "] [ServerHandler handle] [Encryption "
                            "Failed] : [ " +
                            encryption.message + "] ",
                    Log::Level::DEBUG);
            log_->write("[" + to_string(uuid_) + "] [ServerHandler handle] [Encryption Failed] : " +
                                request_->toString(),
                        Log::Level::INFO);
            client_->socket().close();
        }
    } else {
        client_->socket().close();
        return;
    }
}
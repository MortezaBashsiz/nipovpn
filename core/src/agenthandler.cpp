#include "agenthandler.hpp"

#include <sstream>

AgentHandler::AgentHandler(boost::asio::streambuf &readBuffer,
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

AgentHandler::~AgentHandler() {}

void AgentHandler::handle() {
    std::lock_guard lock(mutex_);

    if (!request_->detectType()) {
        log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [NOT HTTP Request] [Request] : " +
                            streambufToString(readBuffer_),
                    Log::Level::DEBUG);
        client_->socketShutdown();
        return;
    }

    if (request_->parsedHttpRequest().target().length() > 0) {
        log_->write("[" + to_string(uuid_) + "] [FORWARD] [SRC " + clientConnStr_ + "] [DST " +
                            std::string(request_->parsedHttpRequest().target()) + "]",
                    Log::Level::INFO);
    }

    if (config_->agent().tlsEnable && !client_->tlsEnabled()) {
        if (!client_->enableTlsClient()) {
            log_->write("[" + to_string(uuid_) + "] [TLS] [ERROR] [Init Client Context] [SRC " +
                                clientConnStr_ + "] [DST " + config_->agent().serverIp + ":" +
                                std::to_string(config_->agent().serverPort) + "]",
                        Log::Level::ERROR);
            client_->socketShutdown();
            return;
        }
    }

    if (!client_->isOpen()) {
        if (!client_->doConnect(config_->agent().serverIp, config_->agent().serverPort)) {
            log_->write("[" + to_string(uuid_) + "] [CONNECT] [ERROR] [To Server] [SRC " +
                                clientConnStr_ + "] [DST " + config_->agent().serverIp + ":" +
                                std::to_string(config_->agent().serverPort) + "]",
                        Log::Level::INFO);
            client_->socketShutdown();
            return;
        }

        if (client_->tlsEnabled() && !client_->doHandshakeClient()) {
            log_->write("[" + to_string(uuid_) + "] [TLS] [ERROR] [Client Handshake] [SRC " +
                                clientConnStr_ + "] [DST " + config_->agent().serverIp + ":" +
                                std::to_string(config_->agent().serverPort) + "]",
                        Log::Level::ERROR);
            client_->socketShutdown();
            return;
        }
    }

    BoolStr encryption{false, std::string("FAILED")};
    encryption = aes256Encrypt(hexStreambufToStr(readBuffer_), config_->general().token);
    if (!encryption.ok) {
        log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Encryption Failed] : [ " +
                            encryption.message + "] ",
                    Log::Level::DEBUG);
        client_->socketShutdown();
        return;
    }

    const std::string innerRequest = encode64(encryption.message);
    const std::string action =
            request_->httpType() == HTTP::HttpType::connect ? "open" : "request";

    std::ostringstream outer;
    outer << config_->general().method + " / HTTP/1.1\r\n"
          << "Host: " << relayHostHeader(config_) << "\r\n"
          << "User-Agent: Mozilla/5.0\r\n"
          << "Accept: */*\r\n"
          << "Content-Type: application/octet-stream\r\n"
          << "X-Nipo-Session: " << to_string(uuid_) << "\r\n"
          << "X-Nipo-Action: " << action << "\r\n"
          << "Content-Length: " << innerRequest.size() << "\r\n"
          << "Connection: close\r\n"
          << "\r\n"
          << innerRequest;

    copyStringToStreambuf(outer.str(), readBuffer_);

    client_->doWrite(readBuffer_);
    client_->doReadAgent();

    if (client_->readBuffer().size() == 0) {
        client_->socketShutdown();
        return;
    }

    const std::string outerResponse = streambufToString(client_->readBuffer());
    const auto bodyPos = outerResponse.find("\r\n\r\n");
    if (bodyPos == std::string::npos) {
        log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [NOT HTTP Response] [Response] : " +
                            outerResponse,
                    Log::Level::DEBUG);
        client_->socketShutdown();
        return;
    }

    const std::string encryptedBody = outerResponse.substr(bodyPos + 4);
    if (encryptedBody.empty()) {
        client_->socketShutdown();
        return;
    }

    BoolStr decryption{false, std::string("FAILED")};
    decryption = aes256Decrypt(encryptedBody, config_->general().token);
    if (!decryption.ok) {
        log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Decryption Failed] : [ " +
                            decryption.message + "] ",
                    Log::Level::DEBUG);
        client_->socketShutdown();
        return;
    }

    copyStringToStreambuf(decryption.message, writeBuffer_);
    log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Decryption Done]",
                Log::Level::DEBUG);

    connect_ = request_->httpType() == HTTP::HttpType::connect;
    end_ = false;
}

std::string AgentHandler::relayHostHeader(const std::shared_ptr<Config> &config) {
    std::string hostHeader = config->general().fakeUrl;
    auto pos = hostHeader.find("://");
    if (pos != std::string::npos) hostHeader = hostHeader.substr(pos + 3);
    pos = hostHeader.find('/');
    if (pos != std::string::npos) hostHeader = hostHeader.substr(0, pos);
    return hostHeader;
}

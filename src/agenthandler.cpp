#include "agenthandler.hpp"

AgentHandler::AgentHandler(boost::asio::streambuf& readBuffer,
                           boost::asio::streambuf& writeBuffer,
                           const std::shared_ptr<Config>& config,
                           const std::shared_ptr<Log>& log,
                           const TCPClient::pointer& client)
    : config_(config),
      log_(log),
      client_(client),
      readBuffer_(readBuffer),
      writeBuffer_(writeBuffer),
      request_(HTTP::create(config, log, readBuffer)) {}

AgentHandler::~AgentHandler() {}

void AgentHandler::handle() {
  BoolStr encryption{false, std::string("FAILED")};
  encryption =
      aes256Encrypt(hexStreambufToStr(readBuffer_), config_->agent().token);
  if (encryption.ok) {
    log_->write("[AgentHandler handle] [Token Valid]", Log::Level::DEBUG);
    std::string newReq(
        request_->genHttpPostReqString(encode64(encryption.message)));
    if (request_->detectType()) {
      log_->write("[AgentHandler handle] [Request] : " + request_->toString(),
                  Log::Level::DEBUG);
      if (!client_->socket().is_open() ||
          request_->httpType() == HTTP::HttpType::http ||
          request_->httpType() == HTTP::HttpType::connect)
        client_->doConnect(config_->agent().serverIp,
                           config_->agent().serverPort);
      copyStringToStreambuf(newReq, readBuffer_);
      log_->write("[AgentHandler handle] [Request To Server] : \n" + newReq,
                  Log::Level::DEBUG);
      client_->doWrite(readBuffer_);
      client_->doRead();
      if (client_->readBuffer().size() > 0) {
        if (request_->httpType() != HTTP::HttpType::connect) {
          HTTP::pointer response =
              HTTP::create(config_, log_, client_->readBuffer());
          if (response->parseHttpResp()) {
            log_->write(
                "[AgentHandler handle] [Response] : " + response->restoString(),
                Log::Level::DEBUG);
            BoolStr decryption{false, std::string("FAILED")};
            decryption =
                aes256Decrypt(decode64(boost::lexical_cast<std::string>(
                                  response->parsedHttpResponse().body())),
                              config_->agent().token);
            if (decryption.ok) {
              copyStringToStreambuf(decryption.message, writeBuffer_);
            } else {
              log_->write("[AgentHandler handle] [Decryption Failed] : [ " +
                              decryption.message + " ] ",
                          Log::Level::DEBUG);
              log_->write("[AgentHandler handle] [Decryption Failed] : " +
                              request_->toString(),
                          Log::Level::INFO);
              client_->socket().close();
            }
          } else {
            log_->write(
                "[AgentHandler handle] [NOT HTTP Response] [Response] : " +
                    streambufToString(client_->readBuffer()),
                Log::Level::DEBUG);
          }
        } else {
          log_->write("[AgentHandler handle] [Response to connect] : \n" +
                          streambufToString(client_->readBuffer()),
                      Log::Level::DEBUG);
          moveStreambuf(client_->readBuffer(), writeBuffer_);
        }
      } else {
        client_->socket().close();
      }
    } else {
      log_->write("[AgentHandler handle] [NOT HTTP Request] [Request] : " +
                      streambufToString(readBuffer_),
                  Log::Level::DEBUG);
    }
  } else {
    log_->write("[AgentHandler handle] [Encryption Faild] : [ " +
                    encryption.message + " ] ",
                Log::Level::DEBUG);
    log_->write(
        "[AgentHandler handle] [Encryption Faild] : " +
            client_->socket().remote_endpoint().address().to_string() + ":" +
            std::to_string(client_->socket().remote_endpoint().port()) + " ] ",
        Log::Level::INFO);
    client_->socket().close();
  }
}
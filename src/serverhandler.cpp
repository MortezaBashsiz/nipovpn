#include "serverhandler.hpp"

ServerHandler::ServerHandler(boost::asio::streambuf& readBuffer,
  boost::asio::streambuf& writeBuffer,
  const std::shared_ptr<Config>& config,
  const std::shared_ptr<Log>& log,
  const TCPClient::pointer& client)
  :
    config_(config),
    log_(log),
    client_(client),
    readBuffer_(readBuffer),
    writeBuffer_(writeBuffer),
    request_(HTTP::create(config, log, readBuffer))
{}

ServerHandler::~ServerHandler()
{}

void ServerHandler::handle()
{
  if (request_->detectType())
  {
    log_->write("[ServerHandler handle] [Request From Agent] : "+request_->toString(), Log::Level::DEBUG);
    BoolStr decryption{false, std::string("FAILED")};
    decryption = aes256Decrypt(
      decode64(
        boost::lexical_cast<std::string>(request_->parsedHttpRequest().body())
      ), 
      config_->agent().token
    );
    if ( decryption.ok )
    {
      log_->write("[ServerHandler handle] [Token Valid] : "+request_->toString(), Log::Level::DEBUG);
      auto tempHexArr = strTohexArr(
        decryption.message
      );
      std::string tempHexArrStr(tempHexArr.begin(), tempHexArr.end());
      copyStringToStreambuf(tempHexArrStr, readBuffer_);
      if (request_->detectType())
      {   
        log_->write("[ServerHandler handle] [Request] : "+request_->toString(), Log::Level::DEBUG);
        switch (request_->httpType()){
          case HTTP::HttpType::connect:
            {
              boost::asio::streambuf tempBuff;
              std::iostream os(&tempBuff);
              client_->doConnect(request_->dstIP(), request_->dstPort());
              if (client_->socket().is_open())
              {
                std::string message("HTTP/1.1 200 Connection established\r\n\r\n");
                os << message;
              } else
              {
                std::string message("HTTP/1.1 500 Connection failed\r\n\r\n");
                os << message;
              }
              moveStreamBuff(tempBuff, writeBuffer_);
            }
            break;
          case HTTP::HttpType::http:
          case HTTP::HttpType::https:
            {
              if (!client_->socket().is_open())
                client_->doConnect(request_->dstIP(), request_->dstPort());
              client_->doWrite(readBuffer_);
              client_->doRead();
              if (client_->readBuffer().size() > 0)
              {
                BoolStr encryption{false, std::string("FAILED")};
                encryption = aes256Encrypt(streambufToString(client_->readBuffer()), config_->agent().token);
                if ( encryption.ok )
                {
                  std::string newRes(
                    request_->genHttpOkResString(
                      encode64(
                        encryption.message
                      )
                    )
                  );
                  copyStringToStreambuf(newRes, writeBuffer_);
                  if (request_->httpType() == HTTP::HttpType::http)
                    client_->socket().close();
                } else 
                {
                  log_->write("[ServerHandler handle] [Encryption Failed] : [ "+ 
                  decryption.message+" ] ",
                  Log::Level::DEBUG
                  );
                  log_->write("[ServerHandler handle] [Encryption Failed] : "+request_->toString(), Log::Level::INFO);
                  client_->socket().close();
                }
              } else {
                client_->socket().close();
              }
            }
            break;
        }
      }else
      {
        log_->write("[ServerHandler handle] [NOT HTTP Request] [Request] : "+ streambufToString(readBuffer_), Log::Level::DEBUG);
      }
    } else
    {
      log_->write("[ServerHandler handle] [Decryption Failed] : [ "+ 
      decryption.message+" ] ",
      Log::Level::DEBUG
      );
      log_->write("[ServerHandler handle] [Decryption Failed] : "+request_->toString(), Log::Level::INFO);
      client_->socket().close();
    }
  } else
  {
    log_->write("[ServerHandler handle] [NOT HTTP Request From Agent] [Request] : "+ streambufToString(readBuffer_), Log::Level::DEBUG);
  }
}
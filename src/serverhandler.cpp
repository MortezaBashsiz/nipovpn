#include "serverhandler.hpp"

/*
 * Constructor to initialize the ServerHandler object.
 * @param readBuffer - Buffer for reading incoming data.
 * @param writeBuffer - Buffer for writing outgoing data.
 * @param config - Shared pointer to the configuration object.
 * @param log - Shared pointer to the logging object.
 * @param client - Shared pointer to the TCP client object.
 */
ServerHandler::ServerHandler(boost::asio::streambuf& readBuffer,
                             boost::asio::streambuf& writeBuffer,
                             const std::shared_ptr<Config>& config,
                             const std::shared_ptr<Log>& log,
                             const TCPClient::pointer& client,
                             const std::string& clientConnStr)
    : config_(config),
      log_(log),
      client_(client),
      readBuffer_(readBuffer),
      writeBuffer_(writeBuffer),
      request_(HTTP::create(config, log, readBuffer)),
      clientConnStr_(clientConnStr)  // Initialize client connection string
{}

/*
 * Destructor for cleanup. No specific resources to release.
 */
ServerHandler::~ServerHandler() {}

/*
 * Handles the incoming request by processing it based on its type (HTTP/HTTPS).
 * It performs decryption, handles connection, and processes the request.
 */
void ServerHandler::handle() {
  // Detects the type of request (HTTP/HTTPS).
  if (request_->detectType()) {
    // Log the received request from the agent.
    log_->write(
        "[ServerHandler handle] [Request From Agent] : " + request_->toString(),
        Log::Level::DEBUG);

    // Decrypt the request body using AES256.
    BoolStr decryption{false, std::string("FAILED")};
    decryption = aes256Decrypt(decode64(boost::lexical_cast<std::string>(
                                   request_->parsedHttpRequest().body())),
                               config_->agent().token);

    if (decryption.ok) {
      // Log successful decryption and process the request.
      log_->write(
          "[ServerHandler handle] [Token Valid] : " + request_->toString(),
          Log::Level::DEBUG);
      auto tempHexArr = strToHexArr(decryption.message);
      std::string tempHexArrStr(tempHexArr.begin(), tempHexArr.end());
      copyStringToStreambuf(tempHexArrStr, readBuffer_);

      // Re-check the request type and process accordingly.
      if (request_->detectType()) {
        log_->write(
            "[ServerHandler handle] [Request] : " + request_->toString(),
            Log::Level::DEBUG);
        switch (request_->httpType()) {
          case HTTP::HttpType::connect: {
            // Handle CONNECT request to establish a connection.
            boost::asio::streambuf tempBuff;
            std::iostream os(&tempBuff);
            client_->doConnect(request_->dstIP(), request_->dstPort());
            log_->write("[CONNECT] [SRC " + clientConnStr_ + "] [DST " +
                            request_->dstIP() + ":" +
                            std::to_string(request_->dstPort()) + "]",
                        Log::Level::INFO);
            if (client_->socket().is_open()) {
              std::string message(
                  "HTTP/1.1 200 Connection established\r\n\r\n");
              os << message;
            } else {
              std::string message("HTTP/1.1 500 Connection failed\r\n\r\n");
              os << message;
            }
            moveStreambuf(tempBuff, writeBuffer_);
          } break;
          case HTTP::HttpType::http:
          case HTTP::HttpType::https: {
            // Handle HTTP or HTTPS requests.
            if (request_->httpType() == HTTP::HttpType::http) {
              client_->doConnect(request_->dstIP(), request_->dstPort());
              log_->write("[CONNECT] [SRC " + clientConnStr_ + "] [DST " +
                              request_->dstIP() + ":" +
                              std::to_string(request_->dstPort()) + "]",
                          Log::Level::INFO);
            }
            if (!client_->socket().is_open()) {
              client_->doConnect(request_->dstIP(), request_->dstPort());
              log_->write("[CONNECT] [SRC " + clientConnStr_ + "] [DST " +
                              request_->dstIP() + ":" +
                              std::to_string(request_->dstPort()) + "]",
                          Log::Level::INFO);
            }
            client_->doWrite(readBuffer_);
            client_->doRead();
            if (client_->readBuffer().size() > 0) {
              // Encrypt the response and send it back.
              BoolStr encryption{false, std::string("FAILED")};
              encryption =
                  aes256Encrypt(streambufToString(client_->readBuffer()),
                                config_->agent().token);
              if (encryption.ok) {
                std::string newRes(
                    request_->genHttpOkResString(encode64(encryption.message)));
                copyStringToStreambuf(newRes, writeBuffer_);
                if (request_->httpType() == HTTP::HttpType::http)
                  client_->socket().close();
              } else {
                // Log encryption failure and close the connection.
                log_->write("[ServerHandler handle] [Encryption Failed] : [ " +
                                decryption.message + "] ",
                            Log::Level::DEBUG);
                log_->write("[ServerHandler handle] [Encryption Failed] : " +
                                request_->toString(),
                            Log::Level::INFO);
                client_->socket().close();
              }
            } else {
              // Close the connection if no data was read.
              client_->socket().close();
            }
          } break;
        }
      } else {
        // Log non-HTTP requests if detected.
        log_->write("[ServerHandler handle] [NOT HTTP Request] [Request] : " +
                        streambufToString(readBuffer_),
                    Log::Level::DEBUG);
      }
    } else {
      // Log decryption failure and close the connection.
      log_->write("[ServerHandler handle] [Decryption Failed] : [ " +
                      decryption.message + "] ",
                  Log::Level::DEBUG);
      log_->write("[ServerHandler handle] [Decryption Failed] : " +
                      request_->toString(),
                  Log::Level::INFO);
      client_->socket().close();
    }
  } else {
    // Log if the request is not from an agent.
    log_->write(
        "[ServerHandler handle] [NOT HTTP Request From Agent] [Request] : " +
            streambufToString(readBuffer_),
        Log::Level::DEBUG);
  }
}

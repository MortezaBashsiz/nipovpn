#include "agenthandler.hpp"

// Constructor for the AgentHandler class
AgentHandler::AgentHandler(boost::asio::streambuf& readBuffer,
                           boost::asio::streambuf& writeBuffer,
                           const std::shared_ptr<Config>& config,
                           const std::shared_ptr<Log>& log,
                           const TCPClient::pointer& client)
    : config_(config),            // Initialize configuration
      log_(log),                  // Initialize logging
      client_(client),            // Initialize TCP client
      readBuffer_(readBuffer),    // Reference to the read buffer
      writeBuffer_(writeBuffer),  // Reference to the write buffer
      request_(HTTP::create(config, log, readBuffer)) {
}  // Initialize HTTP request handler

// Destructor for the AgentHandler class
AgentHandler::~AgentHandler() {}

// Main handler function for processing requests
void AgentHandler::handle() {
  // Initialize encryption status
  BoolStr encryption{false, std::string("FAILED")};

  // Encrypt the request data from the read buffer
  encryption =
      aes256Encrypt(hexStreambufToStr(readBuffer_), config_->agent().token);

  if (encryption.ok) {
    // Log successful token validation
    log_->write("[AgentHandler handle] [Token Valid]", Log::Level::DEBUG);

    // Generate a new HTTP POST request string with the encrypted message
    std::string newReq(
        request_->genHttpPostReqString(encode64(encryption.message)));

    // Check if the request type is valid
    if (request_->detectType()) {
      // Log the HTTP request details
      log_->write("[AgentHandler handle] [Request] : " + request_->toString(),
                  Log::Level::DEBUG);

      // If the client socket is not open or the request type is HTTP or CONNECT
      if (!client_->socket().is_open() ||
          request_->httpType() == HTTP::HttpType::http ||
          request_->httpType() == HTTP::HttpType::connect) {
        // Connect the TCP client to the server
        boost::system::error_code ec;
        client_->doConnect(config_->agent().serverIp,
                           config_->agent().serverPort);

        // Check for connection errors
        if (ec) {
          log_->write(std::string("[AgentHandler handle] Connection error: ") +
                          ec.message(),
                      Log::Level::ERROR);
          return;
        }
      }

      // Copy the new request to the read buffer
      copyStringToStreambuf(newReq, readBuffer_);

      // Log the request to be sent to the server
      log_->write("[AgentHandler handle] [Request To Server] : \n" + newReq,
                  Log::Level::DEBUG);

      // Write the request to the client socket and initiate a read operation
      client_->doWrite(readBuffer_);
      client_->doRead();

      // Check if there is data available in the read buffer
      if (client_->readBuffer().size() > 0) {
        // If the HTTP request type is not CONNECT
        if (request_->httpType() != HTTP::HttpType::connect) {
          // Create an HTTP response handler
          HTTP::pointer response =
              HTTP::create(config_, log_, client_->readBuffer());

          // Parse the HTTP response
          if (response->parseHttpResp()) {
            // Log the response details
            log_->write(
                "[AgentHandler handle] [Response] : " + response->restoString(),
                Log::Level::DEBUG);

            // Decrypt the response body
            BoolStr decryption{false, std::string("FAILED")};
            decryption =
                aes256Decrypt(decode64(boost::lexical_cast<std::string>(
                                  response->parsedHttpResponse().body())),
                              config_->agent().token);

            // Check if decryption was successful
            if (decryption.ok) {
              // Copy the decrypted message to the write buffer
              copyStringToStreambuf(decryption.message, writeBuffer_);
            } else {
              // Log decryption failure and close the socket
              log_->write("[AgentHandler handle] [Decryption Failed] : [ " +
                              decryption.message + " ] ",
                          Log::Level::DEBUG);
              log_->write("[AgentHandler handle] [Decryption Failed] : " +
                              request_->toString(),
                          Log::Level::INFO);
              client_->socket().close();
            }
          } else {
            // Log if the response is not an HTTP response
            log_->write(
                "[AgentHandler handle] [NOT HTTP Response] [Response] : " +
                    streambufToString(client_->readBuffer()),
                Log::Level::DEBUG);
          }
        } else {
          // Log the response to a CONNECT request
          log_->write("[AgentHandler handle] [Response to connect] : \n" +
                          streambufToString(client_->readBuffer()),
                      Log::Level::DEBUG);

          // Move the response from the read buffer to the write buffer
          moveStreambuf(client_->readBuffer(), writeBuffer_);
        }
      } else {
        // Close the socket if no data is available
        client_->socket().close();
      }
    } else {
      // Log if the request is not a valid HTTP request
      log_->write("[AgentHandler handle] [NOT HTTP Request] [Request] : " +
                      streambufToString(readBuffer_),
                  Log::Level::DEBUG);
    }
  } else {
    // Log encryption failure and close the socket
    log_->write("[AgentHandler handle] [Encryption Failed] : [ " +
                    encryption.message + " ] ",
                Log::Level::DEBUG);
    log_->write(
        "[AgentHandler handle] [Encryption Failed] : " +
            client_->socket().remote_endpoint().address().to_string() + ":" +
            std::to_string(client_->socket().remote_endpoint().port()) + " ] ",
        Log::Level::INFO);
    client_->socket().close();
  }
}

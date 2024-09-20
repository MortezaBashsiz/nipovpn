#include "agenthandler.hpp"

// Constructor for the AgentHandler class
AgentHandler::AgentHandler(boost::asio::streambuf &readBuffer,
                           boost::asio::streambuf &writeBuffer,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           const TCPClient::pointer &client,
                           const std::string &clientConnStr,
                           boost::uuids::uuid uuid)
    : config_(config),          // Initialize configuration
      log_(log),                // Initialize logging
      client_(client),          // Initialize TCP client
      readBuffer_(readBuffer),  // Reference to the read buffer
      writeBuffer_(writeBuffer),// Reference to the write buffer
      request_(HTTP::create(config, log, readBuffer, uuid)),
      clientConnStr_(clientConnStr),// Initialize client connection string
      uuid_(uuid) {}

// Destructor for the AgentHandler class
AgentHandler::~AgentHandler() {}

// Main handler function for processing requests
void AgentHandler::handle() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Initialize encryption status
    BoolStr encryption{false, std::string("FAILED")};

    // Encrypt the request data from the read buffer
    encryption =
            aes256Encrypt(hexStreambufToStr(readBuffer_), config_->agent().token);

    if (encryption.ok) {
        // Log successful token validation
        log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Encryption Done]", Log::Level::DEBUG);

        // Generate a new HTTP POST request string with the encrypted message
        std::string newReq(
                request_->genHttpPostReqString(encode64(encryption.message)));

        // Check if the request type is valid
        if (request_->detectType()) {
            // Log the HTTP request details
            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Request] : " + request_->toString(),
                        Log::Level::DEBUG);

            // Log the client connention string and HTTP request target
            if (request_->parsedHttpRequest().target().length() > 0) {
                log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " + clientConnStr_ + "]" + " [DST " +
                                    boost::lexical_cast<std::string>(
                                            request_->parsedHttpRequest().target()) +
                                    "]",
                            Log::Level::INFO);
            }

            // If the client socket is not open or the request type is HTTP or CONNECT
            if (!client_->getSocket().is_open() ||
                request_->httpType() == HTTP::HttpType::http ||
                request_->httpType() == HTTP::HttpType::connect) {
                // Connect the TCP client to the server
                boost::system::error_code ec;
                ;

                if (!client_->doConnect(config_->agent().serverIp,
                                        config_->agent().serverPort)) {
                    log_->write(std::string("[" + to_string(uuid_) + "] [CONNECT] [ERROR] [To Server] [SRC ") +
                                        clientConnStr_ + "] [DST " +
                                        config_->agent().serverIp + ":" +
                                        std::to_string(config_->agent().serverPort) + "]",
                                Log::Level::INFO);
                }

                // Check for connection errors
                if (ec) {
                    log_->write(std::string("[" + to_string(uuid_) + "] [AgentHandler handle] Connection error: ") +
                                        ec.message(),
                                Log::Level::ERROR);
                    return;
                }
            }

            // Copy the new request to the read buffer
            copyStringToStreambuf(newReq, readBuffer_);

            // Log the request to be sent to the server
            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Request To Server] : \n" + newReq,
                        Log::Level::DEBUG);

            // Write the request to the client socket and initiate a read
            // operation
            client_->doWrite(readBuffer_);
            client_->doRead();

            // Check if there is data available in the read buffer
            if (client_->getReadBuffer().size() > 0) {
                // If the HTTP request type is not CONNECT
                if (request_->httpType() != HTTP::HttpType::connect) {
                    // Create an HTTP response handler
                    HTTP::pointer response =
                            HTTP::create(config_, log_, client_->getReadBuffer(), uuid_);

                    // Parse the HTTP response
                    if (response->parseHttpResp()) {
                        // Log the response details
                        log_->write(
                                "[" + to_string(uuid_) + "] [AgentHandler handle] [Response] : " + response->restoString(),
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
                            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Decryption Done]", Log::Level::DEBUG);
                        } else {
                            // Log decryption failure and close the socket
                            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Decryption Failed] : [ " +
                                                decryption.message + "] ",
                                        Log::Level::DEBUG);
                            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Decryption Failed] : " +
                                                request_->toString(),
                                        Log::Level::INFO);
                            client_->getSocket().close();
                        }
                    } else {
                        // Log if the response is not an HTTP response
                        log_->write(
                                "[AgentHandler handle] [NOT HTTP Response] "
                                "[Response] : " +
                                        streambufToString(client_->getReadBuffer()),
                                Log::Level::DEBUG);
                    }
                } else {
                    // Log the response to a CONNECT request
                    log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Response to connect] : \n" +
                                        streambufToString(client_->getReadBuffer()),
                                Log::Level::DEBUG);

                    // Move the response from the read buffer to the write buffer
                    moveStreambuf(client_->getReadBuffer(), writeBuffer_);
                }
            } else {
                // Close the socket if no data is available
                client_->getSocket().close();
                return;
            }
        } else {
            // Log if the request is not a valid HTTP request
            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [NOT HTTP Request] [Request] : " +
                                streambufToString(readBuffer_),
                        Log::Level::DEBUG);
            // Close the socket if no data is available
            client_->getSocket().close();
            return;
        }
    } else {
        // Log encryption failure and close the socket
        log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Encryption Failed] : [ " +
                            encryption.message + "] ",
                    Log::Level::DEBUG);
        log_->write(
                "[AgentHandler handle] [Encryption Failed] : " +
                        client_->getSocket().remote_endpoint().address().to_string() + ":" +
                        std::to_string(client_->getSocket().remote_endpoint().port()) + "] ",
                Log::Level::INFO);
        // Close the socket if no data is available
        client_->getSocket().close();
        return;
    }
}

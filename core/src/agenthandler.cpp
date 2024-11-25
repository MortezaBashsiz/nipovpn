#include "agenthandler.hpp"

/**
 * @brief Constructs an AgentHandler object to manage client-agent interactions.
 *
 * This constructor initializes the AgentHandler with the necessary resources, 
 * including read and write buffers, configuration, logging, client connection, 
 * and a unique identifier for the session.
 *
 * @param readBuffer Reference to the stream buffer used for reading data.
 * @param writeBuffer Reference to the stream buffer used for writing data.
 * @param config Shared pointer to the configuration object containing system settings.
 * @param log Shared pointer to the logging object for recording events and errors.
 * @param client Shared pointer to the TCPClient object managing the client connection.
 * @param clientConnStr A string representing the client's connection details.
 * @param uuid A unique identifier for the session, provided as a Boost UUID.
 *
 * @details
 * - Initializes the HTTP request handler using the provided configuration, logger, 
 *   read buffer, and UUID.
 * - Sets the `end_` flag to `false`, indicating the handler is active and ready.
 * - Initializes the `connect_` flag to `false`, indicating no active connection initially.
 */
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

/**
 * @brief Handles the processing of incoming requests, encryption, server communication, and response handling.
 *
 * This function coordinates multiple tasks, including:
 * - Encrypting the incoming data.
 * - Generating and sending an HTTP request to the server.
 * - Receiving and decrypting the server's response.
 * - Managing the connection state and logging detailed information at each step.
 *
 * @details
 * - The function ensures thread safety using a mutex lock.
 * - The incoming data is encrypted using AES-256 with a token retrieved from the configuration.
 * - If encryption is successful, the function builds an HTTP POST request and determines the request type.
 * - It establishes a connection to the server if required and sends the encrypted request.
 * - The server's response is parsed and decrypted. If successful, the decrypted data is copied to the write buffer.
 * - Handles HTTP and non-HTTP responses, including error scenarios and connection cleanup.
 * - Maintains logging for debugging and informational purposes throughout the process.
 *
 * @note In case of any error during encryption, connection, or response handling, the client socket is closed.
 */
void AgentHandler::handle() {
    std::lock_guard<std::mutex> lock(mutex_);

    BoolStr encryption{false, std::string("FAILED")};

    encryption =
            aes256Encrypt(hexStreambufToStr(readBuffer_), config_->general().token);

    if (encryption.ok) {
        log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Encryption Done]", Log::Level::DEBUG);
        std::string newReq(
                request_->genHttpPostReqString(encode64(encryption.message)));

        if (request_->detectType()) {
            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Request] : " + request_->toString(),
                        Log::Level::DEBUG);
            if (request_->parsedHttpRequest().target().length() > 0) {
                log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " + clientConnStr_ + "]" + " [DST " +
                                    boost::lexical_cast<std::string>(
                                            request_->parsedHttpRequest().target()) +
                                    "]",
                            Log::Level::INFO);
            }
            if (!client_->socket().is_open() ||
                request_->httpType() == HTTP::HttpType::http ||
                request_->httpType() == HTTP::HttpType::connect) {
                connect_ = true;
                boost::system::error_code ec;
                if (!client_->doConnect(config_->agent().serverIp,
                                        config_->agent().serverPort)) {
                    log_->write(std::string("[" + to_string(uuid_) + "] [CONNECT] [ERROR] [To Server] [SRC ") +
                                        clientConnStr_ + "] [DST " +
                                        config_->agent().serverIp + ":" +
                                        std::to_string(config_->agent().serverPort) + "]",
                                Log::Level::INFO);
                }
                if (ec) {
                    log_->write(std::string("[" + to_string(uuid_) + "] [AgentHandler handle] Connection error: ") +
                                        ec.message(),
                                Log::Level::ERROR);
                    return;
                }
            }

            copyStringToStreambuf(newReq, readBuffer_);
            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Request To Server] : \n" + newReq,
                        Log::Level::DEBUG);

            client_->doWrite(readBuffer_);
            client_->doHandle();

            if (client_->readBuffer().size() > 0) {
                if (request_->httpType() != HTTP::HttpType::connect) {
                    HTTP::pointer response =
                            HTTP::create(config_, log_, client_->readBuffer(), uuid_);
                    if (response->parseHttpResp()) {
                        log_->write(
                                "[" + to_string(uuid_) + "] [AgentHandler handle] [Response] : " + response->restoString(),
                                Log::Level::DEBUG);
                        BoolStr decryption{false, std::string("FAILED")};
                        decryption =
                                aes256Decrypt(decode64(boost::lexical_cast<std::string>(
                                                      response->parsedHttpResponse().body())),
                                              config_->general().token);
                        if (boost::lexical_cast<std::string>(response->parsedHttpResponse()[config_->general().chunkHeader]) == "yes") {
                            end_ = true;
                        }
                        if (decryption.ok) {
                            copyStringToStreambuf(decryption.message, writeBuffer_);
                            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Decryption Done]", Log::Level::DEBUG);
                        } else {
                            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Decryption Failed] : [ " +
                                                decryption.message + "] ",
                                        Log::Level::DEBUG);
                            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Decryption Failed] : " +
                                                request_->toString(),
                                        Log::Level::INFO);
                            client_->socket().close();
                        }
                    } else {
                        log_->write(
                                "[AgentHandler handle] [NOT HTTP Response] "
                                "[Response] : " +
                                        streambufToString(client_->readBuffer()),
                                Log::Level::DEBUG);
                    }
                } else {
                    connect_ = true;
                    log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Response to connect] : \n" +
                                        streambufToString(client_->readBuffer()),
                                Log::Level::DEBUG);
                    moveStreambuf(client_->readBuffer(), writeBuffer_);
                }
            } else {
                client_->socket().close();
                return;
            }
        } else {
            log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [NOT HTTP Request] [Request] : " +
                                streambufToString(readBuffer_),
                        Log::Level::DEBUG);

            client_->socket().close();
            return;
        }
    } else {
        log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Encryption Failed] : [ " +
                            encryption.message + "] ",
                    Log::Level::DEBUG);
        log_->write(
                "[AgentHandler handle] [Encryption Failed] : " +
                        client_->socket().remote_endpoint().address().to_string() + ":" +
                        std::to_string(client_->socket().remote_endpoint().port()) + "] ",
                Log::Level::INFO);
        client_->socket().close();
        return;
    }
}

/**
 * @brief Continues reading and processing data from the server, handling encryption, decryption, and responses.
 *
 * This function performs the following tasks:
 * - Generates a REST-based HTTP POST request and sends it to the server.
 * - Handles the server's response, including parsing, decryption, and processing.
 * - Logs detailed information about the request, response, and errors encountered during processing.
 *
 * @details
 * - Thread safety is ensured using a mutex lock.
 * - The request data is generated using the `genHttpRestPostReqString` method and sent to the server.
 * - Upon receiving the server's response, the function parses it as an HTTP response.
 * - The body of the response is decrypted using AES-256, and the decrypted content is stored in the write buffer.
 * - If the response indicates the end of a chunked transfer (`chunkHeader` set to "yes"), the `end_` flag is set to `true`.
 * - Handles both HTTP and non-HTTP responses, logging any issues encountered.
 * - In case of decryption failure or an empty response, the client socket is closed.
 *
 * @note
 * - This function assumes that the client connection is already established and ready for communication.
 * - Errors in decryption, parsing, or an empty response buffer will terminate the connection.
 */
void AgentHandler::continueRead() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string newReq(
            request_->genHttpRestPostReqString());
    copyStringToStreambuf(newReq, readBuffer_);
    client_->doWrite(readBuffer_);
    client_->doHandle();
    if (client_->readBuffer().size() > 0) {
        if (request_->httpType() != HTTP::HttpType::connect) {
            HTTP::pointer response =
                    HTTP::create(config_, log_, client_->readBuffer(), uuid_);
            if (response->parseHttpResp()) {
                log_->write(
                        "[" + to_string(uuid_) + "] [AgentHandler continueRead handle] [Response] : " + response->restoString(),
                        Log::Level::DEBUG);
                BoolStr decryption{false, std::string("FAILED")};
                decryption =
                        aes256Decrypt(decode64(boost::lexical_cast<std::string>(
                                              response->parsedHttpResponse().body())),
                                      config_->general().token);
                if (boost::lexical_cast<std::string>(response->parsedHttpResponse()[config_->general().chunkHeader]) == "yes") {
                    end_ = true;
                }
                if (decryption.ok) {
                    copyStringToStreambuf(decryption.message, writeBuffer_);
                    log_->write("[" + to_string(uuid_) + "] [AgentHandler continueRead handle] [Decryption Done]", Log::Level::DEBUG);
                } else {
                    log_->write("[" + to_string(uuid_) + "] [AgentHandler continueRead handle] [Decryption Failed] : [ " +
                                        decryption.message + "] ",
                                Log::Level::DEBUG);
                    log_->write("[" + to_string(uuid_) + "] [AgentHandler continueRead handle] [Decryption Failed] : " +
                                        request_->toString(),
                                Log::Level::INFO);
                    client_->socket().close();
                }
            } else {
                log_->write(
                        "[AgentHandler continueRead handle] [NOT HTTP Response] "
                        "[Response] : " +
                                streambufToString(client_->readBuffer()),
                        Log::Level::DEBUG);
            }
        } else {
            connect_ = true;
            log_->write("[" + to_string(uuid_) + "] [AgentHandler continueRead handle] [Response to connect] : \n" +
                                streambufToString(client_->readBuffer()),
                        Log::Level::DEBUG);
            moveStreambuf(client_->readBuffer(), writeBuffer_);
        }
    } else {
        client_->socket().close();
        return;
    }
}

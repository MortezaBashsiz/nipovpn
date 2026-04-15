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
    std::lock_guard lock(mutex_);

    BoolStr encryption{false, std::string("FAILED")};
    encryption = aes256Encrypt(hexStreambufToStr(readBuffer_),
                               config_->general().token);
    if (!encryption.ok) {
        log_->write("[" + to_string(uuid_) +
                            "] [AgentHandler handle] [Encryption Failed] : [ " +
                            encryption.message + "] ",
                    Log::Level::DEBUG);
        client_->socketShutdown();
        return;
    }

    log_->write("[" + to_string(uuid_) + "] [AgentHandler handle] [Encryption Done]",
                Log::Level::DEBUG);

    std::string newReq(
            request_->genHttpPostReqString(encode64(encryption.message)));

    if (!request_->detectType()) {
        log_->write("[" + to_string(uuid_) +
                            "] [AgentHandler handle] [NOT HTTP Request] [Request] : " +
                            streambufToString(readBuffer_),
                    Log::Level::DEBUG);
        client_->socketShutdown();
        return;
    }

    if (request_->parsedHttpRequest().target().length() > 0) {
        log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " + clientConnStr_ +
                            "] [DST " +
                            std::string(request_->parsedHttpRequest().target()) +
                            "]",
                    Log::Level::INFO);
    }

    if (config_->agent().tlsEnable && !client_->tlsEnabled()) {
        if (!client_->enableTlsClient()) {
            log_->write("[" + to_string(uuid_) +
                                "] [TLS] [ERROR] [Init Client Context] [SRC " +
                                clientConnStr_ + "] [DST " + config_->agent().serverIp +
                                ":" + std::to_string(config_->agent().serverPort) + "]",
                        Log::Level::ERROR);
            client_->socketShutdown();
            return;
        }
    }

    if (!client_->isOpen()) {
        connect_ = true;

        if (!client_->doConnect(config_->agent().serverIp,
                                config_->agent().serverPort)) {
            log_->write("[" + to_string(uuid_) +
                                "] [CONNECT] [ERROR] [To Server] [SRC " + clientConnStr_ +
                                "] [DST " + config_->agent().serverIp + ":" +
                                std::to_string(config_->agent().serverPort) + "]",
                        Log::Level::INFO);
            client_->socketShutdown();
            return;
        }

        if (client_->tlsEnabled()) {
            if (!client_->doHandshakeClient()) {
                log_->write("[" + to_string(uuid_) +
                                    "] [TLS] [ERROR] [Client Handshake] [SRC " +
                                    clientConnStr_ + "] [DST " + config_->agent().serverIp +
                                    ":" + std::to_string(config_->agent().serverPort) + "]",
                            Log::Level::ERROR);
                client_->socketShutdown();
                return;
            }
        }
    }

    copyStringToStreambuf(newReq, readBuffer_);
    client_->doWrite(readBuffer_);
    client_->doReadAgent();

    if (client_->readBuffer().size() == 0) {
        client_->socketShutdown();
        return;
    }

    if (request_->httpType() == HTTP::HttpType::connect) {
        connect_ = true;
        moveStreambuf(client_->readBuffer(), writeBuffer_);
        return;
    }

    HTTP::pointer response =
            HTTP::create(config_, log_, client_->readBuffer(), uuid_);
    if (!response->parseHttpResp()) {
        log_->write("[AgentHandler handle] [NOT HTTP Response] [Response] : " +
                            streambufToString(client_->readBuffer()),
                    Log::Level::DEBUG);
        client_->socketShutdown();
        return;
    }

    BoolStr decryption{false, std::string("FAILED")};
    decryption = aes256Decrypt(
            decode64(std::string(response->parsedHttpResponse().body())),
            config_->general().token);

    if (!decryption.ok) {
        log_->write("[" + to_string(uuid_) +
                            "] [AgentHandler handle] [Decryption Failed] : [ " +
                            decryption.message + "] ",
                    Log::Level::DEBUG);
        client_->socketShutdown();
        return;
    }

    copyStringToStreambuf(decryption.message, writeBuffer_);
}

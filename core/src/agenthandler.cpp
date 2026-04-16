#include "agenthandler.hpp"

/**
 * @brief Constructs an `AgentHandler` instance with the required buffers, configuration,
 *        logging, client connection, and session identifier.
 *
 * @details
 * - Initializes internal references to the read and write buffers.
 * - Stores shared references to the configuration and logging objects.
 * - Stores the associated `TCPClient` instance used for server communication.
 * - Creates an `HTTP` helper object for parsing and generating HTTP messages.
 * - Saves the client connection string and UUID for logging and tracking purposes.
 * - Sets the initial state flags:
 *   - `end_` is initialized to `false`, indicating processing is not finished.
 *   - `connect_` is initialized to `false`, indicating that no CONNECT tunnel is active.
 *
 * @param readBuffer Reference to the stream buffer used for reading incoming client data.
 * @param writeBuffer Reference to the stream buffer used for storing outgoing response data.
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging object.
 * @param client Shared pointer to the `TCPClient` used for communication with the remote server.
 * @param clientConnStr String describing the client connection source.
 * @param uuid Unique identifier for this handler session, represented as a Boost UUID.
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

/**
 * @brief Destroys the `AgentHandler` instance.
 *
 * @details
 * - Performs no additional cleanup explicitly.
 * - Resource lifetime is managed by referenced objects and smart pointers.
 */
AgentHandler::~AgentHandler() {}

/**
 * @brief Handles a client request by encrypting it, forwarding it to the agent server,
 *        and processing the server response.
 *
 * @details
 * - Ensures thread-safe execution using an internal mutex.
 * - Reads the incoming request data from `readBuffer_` and encrypts it using AES-256.
 * - Encodes the encrypted payload with Base64 and wraps it into an HTTP POST request.
 * - Detects the type of the original HTTP request before forwarding it.
 * - Logs connection details when a valid HTTP target is available.
 * - Initializes TLS on the client connection when enabled in the configuration.
 * - Establishes a TCP connection to the configured server if one is not already open.
 * - Performs a TLS handshake when TLS is active on the client connection.
 * - Sends the generated HTTP request to the remote server and reads the response.
 * - Handles CONNECT requests specially by forwarding the raw response directly.
 * - Parses normal HTTP responses, decrypts their body content, and writes the
 *   decrypted result into `writeBuffer_`.
 * - On any failure during encryption, connection, parsing, decryption, or I/O,
 *   the client socket is shut down and processing stops.
 *
 * @note
 * - For CONNECT requests, the method does not decrypt the response body and instead
 *   forwards the raw response stream directly to the write buffer.
 * - The `connect_` flag is updated when a connection or CONNECT tunnel is established.
 */
void AgentHandler::handle() {
    std::lock_guard lock(mutex_);

    BoolStr encryption{false, std::string("FAILED")};

    /**
     * @brief Encrypts the incoming request payload using the configured token.
     */
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

    /**
     * @brief Builds a new HTTP POST request containing the encrypted payload.
     */
    std::string newReq(
            request_->genHttpPostReqString(encode64(encryption.message)));

    /**
     * @brief Verifies that the incoming request is a valid HTTP request before forwarding.
     */
    if (!request_->detectType()) {
        log_->write("[" + to_string(uuid_) +
                            "] [AgentHandler handle] [NOT HTTP Request] [Request] : " +
                            streambufToString(readBuffer_),
                    Log::Level::DEBUG);
        client_->socketShutdown();
        return;
    }

    /**
     * @brief Logs the source and destination information when the request target is available.
     */
    if (request_->parsedHttpRequest().target().length() > 0) {
        log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " + clientConnStr_ +
                            "] [DST " +
                            std::string(request_->parsedHttpRequest().target()) +
                            "]",
                    Log::Level::INFO);
    }

    /**
     * @brief Enables TLS on the client connection if configured and not already active.
     */
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

    /**
     * @brief Establishes the TCP connection and performs the TLS handshake if needed.
     */
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

    /**
     * @brief Sends the generated request to the remote server and waits for the response.
     */
    copyStringToStreambuf(newReq, readBuffer_);
    client_->doWrite(readBuffer_);
    client_->doReadAgent();

    /**
     * @brief Stops processing if no response data was received from the server.
     */
    if (client_->readBuffer().size() == 0) {
        client_->socketShutdown();
        return;
    }

    /**
     * @brief Handles CONNECT requests by forwarding the raw response directly.
     */
    if (request_->httpType() == HTTP::HttpType::connect) {
        connect_ = true;
        moveStreambuf(client_->readBuffer(), writeBuffer_);
        return;
    }

    /**
     * @brief Creates and parses an HTTP response object from the received server data.
     */
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

    /**
     * @brief Decodes and decrypts the HTTP response body using the configured token.
     */
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

    /**
     * @brief Writes the decrypted response payload into the output buffer.
     */
    copyStringToStreambuf(decryption.message, writeBuffer_);
}
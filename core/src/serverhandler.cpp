#include "serverhandler.hpp"

/**
 * @brief Constructs a `ServerHandler` instance with buffers, configuration,
 *        logging, client connection, and session identifier.
 *
 * @details
 * - Stores references to the configuration, logging, and client objects.
 * - Stores references to the read and write stream buffers.
 * - Creates an internal `HTTP` helper object for parsing and generating HTTP data.
 * - Stores the client connection string and UUID for logging and tracing.
 * - Initializes the internal state flags:
 *   - `end_` is set to `false`
 *   - `connect_` is set to `false`
 *
 * @param readBuffer Reference to the input stream buffer.
 * @param writeBuffer Reference to the output stream buffer.
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging object.
 * @param client Shared pointer to the TCP client used for remote communication.
 * @param clientConnStr String describing the client connection source.
 * @param uuid Unique identifier for this handler instance.
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
 * @brief Destroys the `ServerHandler` instance.
 *
 * @details
 * - Performs no explicit cleanup.
 * - Resource management is handled by referenced objects and smart pointers.
 */
ServerHandler::~ServerHandler() {}

/**
 * @brief Handles an incoming agent request, decrypts it, forwards it to the target,
 *        and prepares the response.
 *
 * @details
 * - Ensures thread-safe execution using an internal mutex.
 * - Parses the incoming request received from the agent.
 * - Decrypts the HTTP request body using the configured token.
 * - Converts the decrypted hexadecimal payload back into raw request data.
 * - Re-parses the decrypted request as HTTP or TLS-related traffic.
 * - Handles requests based on detected type:
 *   - `CONNECT`: Establishes a tunnel and returns a connection status response.
 *   - `HTTP` / `HTTPS`: Connects to the destination, forwards the request,
 *     reads the remote response, encrypts it, and wraps it in an HTTP `200 OK` response.
 * - On any parsing, decryption, encryption, or connection failure, closes the client socket.
 */
void ServerHandler::handle() {
    std::lock_guard lock(mutex_);

    /**
     * @brief Parses the outer request received from the agent.
     */
    if (!request_->detectType()) {
        log_->write("[" + to_string(uuid_) +
                            "] [ServerHandler handle] [NOT HTTP Request From Agent] "
                            "[Request] : " +
                            streambufToString(readBuffer_),
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    BoolStr decryption{false, std::string("FAILED")};

    /**
     * @brief Decrypts the received request body using the configured token.
     */
    decryption = aes256Decrypt(
            decode64(std::string(request_->parsedHttpRequest().body())),
            config_->general().token);

    if (!decryption.ok) {
        log_->write("[" + to_string(uuid_) +
                            "] [ServerHandler handle] [Decryption Failed] : [ " +
                            decryption.message + "] ",
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    /**
     * @brief Converts the decrypted hexadecimal payload into raw request bytes
     *        and replaces the input buffer contents.
     */
    auto tempHexArr = strToHexArr(decryption.message);
    std::string tempHexArrStr(tempHexArr.begin(), tempHexArr.end());
    copyStringToStreambuf(tempHexArrStr, readBuffer_);

    /**
     * @brief Parses the decrypted inner request.
     */
    if (!request_->detectType()) {
        log_->write("[" + to_string(uuid_) +
                            "] [ServerHandler handle] [NOT HTTP Request] [Request] : " +
                            streambufToString(readBuffer_),
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    /**
     * @brief Processes the request according to its detected type.
     */
    switch (request_->httpType()) {
        case HTTP::HttpType::connect: {
            connect_ = true;

            boost::asio::streambuf tempBuff;
            std::iostream os(&tempBuff);

            /**
             * @brief Establishes a CONNECT tunnel to the requested destination.
             */
            if (client_->doConnect(request_->dstIP(), request_->dstPort())) {
                log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " +
                                    clientConnStr_ + "] [DST " + request_->dstIP() + ":" +
                                    std::to_string(request_->dstPort()) + "]",
                            Log::Level::INFO);
                os << "HTTP/1.1 200 Connection established COMP\r\n\r\n";
            } else {
                log_->write(std::string("[") + to_string(uuid_) +
                                    "] [CONNECT] [ERROR] [Resolving Host] [SRC " +
                                    clientConnStr_ + "] [DST " + request_->dstIP() + ":" +
                                    std::to_string(request_->dstPort()) + "]",
                            Log::Level::INFO);
                os << "HTTP/1.1 500 Connection failed COMP\r\n\r\n";
            }

            moveStreambuf(tempBuff, writeBuffer_);
            return;
        }

        case HTTP::HttpType::http:
        case HTTP::HttpType::https: {
            /**
             * @brief Connects to the target destination if no active socket is open.
             */
            if (!client_->socket().is_open()) {
                if (!client_->doConnect(request_->dstIP(), request_->dstPort())) {
                    client_->socket().close();
                    return;
                }
            }

            /**
             * @brief Forwards the request to the destination and reads the response.
             */
            client_->doWrite(readBuffer_);
            client_->doReadServer();

            if (client_->readBuffer().size() == 0) {
                client_->socket().close();
                return;
            }

            BoolStr encryption{false, std::string("FAILED")};

            /**
             * @brief Encrypts the received response payload.
             */
            encryption =
                    aes256Encrypt(streambufToString(client_->readBuffer()),
                                  config_->general().token);

            if (!encryption.ok) {
                log_->write("[" + to_string(uuid_) +
                                    "] [ServerHandler handle] [Encryption Failed] : [ " +
                                    encryption.message + "] ",
                            Log::Level::DEBUG);
                client_->socket().close();
                return;
            }

            /**
             * @brief Wraps the encrypted response in an HTTP `200 OK` response
             *        and writes it to the output buffer.
             */
            std::string newRes(
                    request_->genHttpOkResString(encode64(encryption.message)));
            copyStringToStreambuf(newRes, writeBuffer_);

            connect_ = true;
            end_ = false;
            return;
        }
    }
}
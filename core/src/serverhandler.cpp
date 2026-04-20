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

    // Parse outer request from agent
    if (!request_->detectType()) {
        log_->write("[" + to_string(uuid_) +
                            "] [ServerHandler handle] [NOT HTTP Request From Agent] "
                            "[Request] : " +
                            streambufToString(readBuffer_),
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    const auto &outerReq = request_->parsedHttpRequest();
    const std::string method = std::string(outerReq.method_string());
    const std::string target = std::string(outerReq.target());

    if (method != "POST" || target != "/relay") {
        copyStringToStreambuf(
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n\r\n",
                writeBuffer_);
        return;
    }

    // Extract inner raw request from outer HTTP body
    const std::string innerRequest = std::string(outerReq.body());

    // Split inner payload into:
    //   1) the HTTP message headers (CONNECT or normal HTTP request)
    //   2) any extra bytes already coalesced after the HTTP headers
    //
    // For CONNECT, those extra bytes are usually the first tunneled TLS bytes
    // and MUST be forwarded to the remote destination after connect succeeds.
    const std::size_t headerEnd = innerRequest.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        log_->write("[" + to_string(uuid_) +
                            "] [ServerHandler handle] [INNER REQUEST MISSING HEADER END]",
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    const std::string innerHeaders = innerRequest.substr(0, headerEnd + 4);
    const std::string pendingTunnelData = innerRequest.substr(headerEnd + 4);

    // Parse only the HTTP message part.
    copyStringToStreambuf(innerHeaders, readBuffer_);
    HTTP::pointer inner = HTTP::create(config_, log_, readBuffer_, uuid_);

    if (!inner->detectType()) {
        log_->write("[" + to_string(uuid_) +
                            "] [ServerHandler handle] [NOT INNER HTTP Request] [Request] : " +
                            streambufToString(readBuffer_),
                    Log::Level::DEBUG);
        client_->socket().close();
        return;
    }

    switch (inner->httpType()) {
        case HTTP::HttpType::connect: {
            if (client_->doConnect(inner->dstIP(), inner->dstPort())) {
                log_->write("[" + to_string(uuid_) + "] [CONNECT] [SRC " + clientConnStr_ +
                                    "] [DST " + inner->dstIP() + ":" +
                                    std::to_string(inner->dstPort()) + "]",
                            Log::Level::INFO);

                // If the agent already delivered bytes after the CONNECT headers,
                // they belong to the tunnel and must be forwarded immediately.
                if (!pendingTunnelData.empty()) {
                    boost::asio::streambuf pendingBuf;
                    copyStringToStreambuf(pendingTunnelData, pendingBuf);
                    client_->doWrite(pendingBuf);
                }

                const std::string connectEstablished =
                        "HTTP/1.1 200 Connection Established\r\n\r\n";

                std::ostringstream outer;
                outer << "HTTP/1.1 200 OK\r\n"
                      << "Content-Type: application/octet-stream\r\n"
                      << "Content-Length: " << connectEstablished.size() << "\r\n"
                      << "Connection: keep-alive\r\n"
                      << "\r\n"
                      << connectEstablished;

                copyStringToStreambuf(outer.str(), writeBuffer_);
                connect_ = true;
                end_ = false;
                return;
            } else {
                log_->write("[" + to_string(uuid_) +
                                    "] [CONNECT] [ERROR] [Resolving Host] [SRC " +
                                    clientConnStr_ + "] [DST " + inner->dstIP() + ":" +
                                    std::to_string(inner->dstPort()) + "]",
                            Log::Level::INFO);

                const std::string connectFailed =
                        "HTTP/1.1 502 Bad Gateway\r\n\r\n";

                std::ostringstream outer;
                outer << "HTTP/1.1 502 Bad Gateway\r\n"
                      << "Content-Type: application/octet-stream\r\n"
                      << "Content-Length: " << connectFailed.size() << "\r\n"
                      << "Connection: close\r\n"
                      << "\r\n"
                      << connectFailed;

                copyStringToStreambuf(outer.str(), writeBuffer_);
                connect_ = false;
                end_ = true;
                return;
            }
        }

        case HTTP::HttpType::http:
        case HTTP::HttpType::https: {
            // For normal HTTP requests, forward the full inner request
            // (headers + optional body) exactly as received.
            copyStringToStreambuf(innerRequest, readBuffer_);

            if (!client_->socket().is_open()) {
                if (!client_->doConnect(inner->dstIP(), inner->dstPort())) {
                    client_->socket().close();
                    return;
                }
            }

            client_->doWrite(readBuffer_);
            client_->doReadServer();

            if (client_->readBuffer().size() == 0) {
                client_->socket().close();
                return;
            }

            const std::string innerResponse = streambufToString(client_->readBuffer());

            std::ostringstream outer;
            outer << "HTTP/1.1 200 OK\r\n"
                  << "Content-Type: application/octet-stream\r\n"
                  << "Content-Length: " << innerResponse.size() << "\r\n"
                  << "Connection: keep-alive\r\n"
                  << "\r\n"
                  << innerResponse;

            copyStringToStreambuf(outer.str(), writeBuffer_);
            connect_ = false;
            end_ = false;
            return;
        }
    }
}
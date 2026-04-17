#include "http.hpp"

/**
 * @brief Constructs an `HTTP` instance with configuration, logging, input buffer,
 *        and session identifier.
 *
 * @details
 * - Stores references to the configuration, logging, and stream buffer objects.
 * - Initializes the parsed HTTP request object and default HTTP type.
 * - Initializes the parsed TLS request state with empty values and a default
 *   TLS handshake type.
 * - Stores the UUID used for logging and tracing this session.
 * - Sets the default chunk header state to `"no"`.
 *
 * @param config Shared pointer to the configuration object.
 * @param log Shared pointer to the logging object.
 * @param buffer Reference to the stream buffer containing raw request or response data.
 * @param uuid Unique identifier for the current session.
 */
HTTP::HTTP(const std::shared_ptr<Config> &config,
           const std::shared_ptr<Log> &log,
           boost::asio::streambuf &buffer,
           boost::uuids::uuid uuid)
    : config_(config),
      log_(log),
      buffer_(buffer),
      parsedHttpRequest_(),
      httpType_(HTTP::HttpType::https),
      parsedTlsRequest_{"", "", TlsTypes::TLSHandshake},
      uuid_(uuid) {
    chunkHeader_ = "no";
}

/**
 * @brief Copy-constructs an `HTTP` instance from another instance.
 *
 * @details
 * - Copies references to the configuration, logging, and stream buffer objects.
 * - Copies the parsed HTTP request and parsed TLS request state.
 * - Copies the UUID for session identification.
 * - Resets the chunk header state to `"no"`.
 *
 * @param http Existing `HTTP` object to copy.
 */
HTTP::HTTP(const HTTP &http)
    : config_(http.config_),
      log_(http.log_),
      buffer_(http.buffer_),
      parsedHttpRequest_(http.parsedHttpRequest_),
      parsedTlsRequest_(http.parsedTlsRequest_),
      uuid_(http.uuid_) {
    chunkHeader_ = "no";
}

/**
 * @brief Destroys the `HTTP` instance.
 *
 * @details
 * - Performs no explicit cleanup.
 * - Resource management is handled by referenced objects and standard class members.
 */
HTTP::~HTTP() {}

/**
 * @brief Detects whether the current buffer contains TLS-related traffic or HTTP traffic.
 *
 * @details
 * - Reads the input buffer as a hexadecimal string.
 * - Inspects the first byte to distinguish TLS record types from plain HTTP data.
 * - Treats values `16`, `14`, and `17` as TLS record types:
 *   - `16`: Handshake
 *   - `14`: ChangeCipherSpec
 *   - `17`: ApplicationData
 * - For TLS traffic, stores the raw body and invokes `parseTls()`.
 * - For non-TLS traffic, invokes `parseHttp()`.
 *
 * @return `true` if the request type is successfully detected and parsed, otherwise `false`.
 */
bool HTTP::detectType() {
    std::string requestStr{hexStreambufToStr(buffer_)};
    std::string tmpStr;
    unsigned short pos = 0;

    tmpStr = requestStr.substr(pos, 2);

    if (tmpStr == "16" || tmpStr == "14" || tmpStr == "17") {
        httpType(HTTP::HttpType::https);
        parsedTlsRequest_.body = requestStr;

        if (parseTls())
            return true;
        else
            return false;
    } else {
        if (parseHttp()) {
            return true;
        } else
            return false;
    }
}

/**
 * @brief Parses an HTTP request from the current input buffer.
 *
 * @details
 * - Converts the stream buffer contents to a string.
 * - Uses Boost.Beast request parsing to parse the HTTP request.
 * - Detects whether the request method is `CONNECT` or a normal HTTP method.
 * - Sets the internal HTTP type accordingly.
 * - Extracts and stores the destination IP address and port.
 * - Logs parsing errors when request parsing fails.
 *
 * @return `true` if parsing succeeds, otherwise `false`.
 */
bool HTTP::parseHttp() {
    std::string requestStr(streambufToString(buffer_));
    boost::beast::http::request_parser<boost::beast::http::string_body> parser;
    parser.eager(true);
    boost::beast::error_code error;

    parser.put(boost::asio::buffer(requestStr), error);

    if (error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [HTTP parseHttp] ") + error.what(),
                    Log::Level::DEBUG);
        return false;
    } else {
        parsedHttpRequest_ = parser.get();

        if (parsedHttpRequest_.method() == boost::beast::http::verb::connect)
            httpType(HTTP::HttpType::connect);
        else
            httpType(HTTP::HttpType::http);

        setIPPort();
        return true;
    }
}

/**
 * @brief Parses an HTTP response from the current input buffer.
 *
 * @details
 * - Converts the stream buffer contents to a string.
 * - Uses the internal Boost.Beast parser to parse the HTTP response.
 * - Stores the parsed response object on success.
 * - Logs parsing errors when response parsing fails.
 *
 * @return `true` if parsing succeeds, otherwise `false`.
 */
bool HTTP::parseHttpResp() {
    std::string requestStr(streambufToString(buffer_));
    parser_.eager(true);
    boost::beast::error_code error;

    parser_.put(boost::asio::buffer(requestStr), error);

    if (error) {
        log_->write(std::string("[" + to_string(uuid_) + "] [HTTP parseHttpResp] ") + error.what(),
                    Log::Level::DEBUG);
        return false;
    } else {
        parsedHttpResponse_ = parser_.release();
        return true;
    }
}

/**
 * @brief Parses TLS request data from the stored TLS body.
 *
 * @details
 * - Inspects the TLS record type from the first byte of the hex-encoded body.
 * - Supports parsing of:
 *   - TLS Handshake
 *   - ChangeCipherSpec
 *   - ApplicationData
 * - For TLS handshake records, attempts to extract the Server Name Indication (SNI)
 *   from the ClientHello message.
 * - Updates destination IP and port after successful parsing.
 *
 * @return `true` if parsing succeeds, otherwise `false`.
 */
bool HTTP::parseTls() {
    std::string tmpStr;
    unsigned short pos = 0;

    tmpStr = parsedTlsRequest_.body.substr(pos, 2);

    if (tmpStr == "16") {
        parsedTlsRequest_.type = TlsTypes::TLSHandshake;
        pos = 10;
        tmpStr = parsedTlsRequest_.body.substr(pos, 2);

        if (tmpStr == "01") {
            unsigned short tmpPos(0);

            pos = 86;
            tmpStr = parsedTlsRequest_.body.substr(pos, 2);
            tmpPos = hexToInt(tmpStr);

            pos += 2;
            tmpStr = parsedTlsRequest_.body.substr(pos, tmpPos);
            pos = pos + (tmpPos * 2);

            tmpStr = parsedTlsRequest_.body.substr(pos, 4);
            tmpPos = hexToInt(tmpStr);
            pos += 4;
            pos = pos + (tmpPos * 2);

            tmpStr = parsedTlsRequest_.body.substr(pos, 2);
            tmpPos = hexToInt(tmpStr);
            pos += 2;
            pos = pos + (tmpPos * 2);

            tmpStr = parsedTlsRequest_.body.substr(pos, 4);
            tmpPos = hexToInt(tmpStr);
            pos += 4;

            tmpStr = parsedTlsRequest_.body.substr(pos, 4);
            if (hexToInt(tmpStr) == 0) {
                pos += 14;
                tmpStr = parsedTlsRequest_.body.substr(pos, 4);
                tmpPos = hexToInt(tmpStr);
                pos += 4;
                tmpStr = parsedTlsRequest_.body.substr(pos, tmpPos * 2);
                parsedTlsRequest_.sni = hexToASCII(tmpStr);
            }
        }

        setIPPort();
        return true;

    } else if (tmpStr == "14") {
        parsedTlsRequest_.type = TlsTypes::ChangeCipherSpec;
        setIPPort();
        return true;

    } else if (tmpStr == "17") {
        parsedTlsRequest_.type = TlsTypes::ApplicationData;
        setIPPort();
        return true;

    } else
        return false;
}

/**
 * @brief Generates an HTTP POST request string containing the provided body.
 *
 * @details
 * - Builds an HTTP request using configured method, fake URL, HTTP version,
 *   and User-Agent.
 * - Sets common headers including `Host`, `Accept`, `Connection`,
 *   `Content-Length`, and `Content-Type`.
 * - Appends the payload body followed by the application terminator marker.
 *
 * @param body Body content to include in the HTTP request.
 * @return Serialized HTTP POST request string.
 */
const std::string HTTP::genHttpPostReqString(const std::string &body) const {
    return std::string(config_->general().method + " " +
                       config_->general().fakeUrl + " HTTP/" +
                       config_->agent().httpVersion + "\r\n") +
           "Host: " + config_->general().fakeUrl + "\r\n" +
           "User-Agent: " + config_->agent().userAgent + "\r\n" +
           "Accept: */*\r\n" + "Connection: keep-alive\r\n" +
           "Content-Length: " + std::to_string(body.length()) + "\r\n" +
           "Content-Type: application/x-www-form-urlencoded\r\n" + "\r\n" + body + "COMP\r\n\r\n";
}

/**
 * @brief Generates a REST-style HTTP POST request string.
 *
 * @details
 * - Builds an HTTP request using configured method, fake URL, HTTP version,
 *   and User-Agent.
 * - Adds the custom `Rest: yes` header to mark the request as a REST-style operation.
 * - Appends the application terminator marker.
 *
 * @return Serialized REST-style HTTP POST request string.
 */
const std::string HTTP::genHttpRestPostReqString() const {
    return std::string(config_->general().method + " " +
                       config_->general().fakeUrl + " HTTP/" +
                       config_->agent().httpVersion + "\r\n") +
           "Host: " + config_->general().fakeUrl + "\r\n" +
           "User-Agent: " + config_->agent().userAgent + "\r\n" +
           "Accept: */*\r\n" + "Connection: keep-alive\r\n" +
           "Rest: yes\r\n" + "COMP\r\n\r\n";
}

/**
 * @brief Generates an HTTP `200 OK` response string.
 *
 * @details
 * - Builds a successful HTTP response with content type, content length,
 *   custom chunk header, and cache-related headers.
 * - Appends the response body followed by the application terminator marker.
 *
 * @param body Body content to include in the HTTP response.
 * @return Serialized HTTP `200 OK` response string.
 */
const std::string HTTP::genHttpOkResString(const std::string &body) const {
    return std::string("HTTP/1.1 200 OK\r\n") +
           "Content-Type: application/x-www-form-urlencoded\r\n" +
           "Content-Length: " + std::to_string(body.length()) + "\r\n" +
           config_->general().chunkHeader + ": " + chunkHeader_ + "\r\n" +
           "Connection: keep-alive\r\n" + "Cache-Control: no-cache\r\n" +
           "Pragma: no-cache\r\n" + "\r\n" + body + "COMP\r\n\r\n";
}

/**
 * @brief Extracts and stores destination IP address and port from the parsed request data.
 *
 * @details
 * - For TLS traffic, uses the parsed SNI value and defaults to port `443`
 *   when no explicit port is provided.
 * - For HTTP traffic, extracts the host and optional port from the request target
 *   and defaults to port `80` when no port is present.
 * - For CONNECT traffic, splits the target directly into host and port.
 * - Logs an error when HTTP request parsing does not provide a usable target.
 */
void HTTP::setIPPort() {
    std::string target{};
    std::vector<std::string> splitted;

    switch (httpType()) {
        case HTTP::HttpType::https: {
            target = parsedTlsRequest_.sni;
            splitted = splitString(target, ":");

            if (!splitted.empty()) {
                dstIP_ = splitted[0];
                if (splitted.size() > 1 && !splitted[1].empty()) {
                    dstPort_ = std::stoi(splitted[1]);
                } else {
                    dstPort_ = 443;
                }
            } else {
                dstIP_.clear();
                dstPort_ = 443;
            }
            break;
        }

        case HTTP::HttpType::http: {
            target = boost::lexical_cast<std::string>(parsedHttpRequest_.target());

            // Absolute-form request target:
            //   http://host:port/path
            // Origin-form request target:
            //   /path
            // For origin-form, use Host header instead.
            std::string hostPort;

            if (target.rfind("http://", 0) == 0) {
                std::string withoutScheme = target.substr(std::string("http://").size());
                auto slashPos = withoutScheme.find('/');
                hostPort = (slashPos == std::string::npos)
                                   ? withoutScheme
                                   : withoutScheme.substr(0, slashPos);
            } else if (target.rfind("https://", 0) == 0) {
                std::string withoutScheme = target.substr(std::string("https://").size());
                auto slashPos = withoutScheme.find('/');
                hostPort = (slashPos == std::string::npos)
                                   ? withoutScheme
                                   : withoutScheme.substr(0, slashPos);
            } else {
                // origin-form like "/relay"
                auto hostIt = parsedHttpRequest_.find("Host");
                if (hostIt != parsedHttpRequest_.end()) {
                    hostPort = boost::lexical_cast<std::string>(hostIt->value());
                } else {
                    log_->write("[" + to_string(uuid_) +
                                        "] [HTTP setIPPort] missing Host header",
                                Log::Level::DEBUG);
                    dstIP_.clear();
                    dstPort_ = 80;
                    break;
                }
            }

            splitted = splitString(hostPort, ":");
            if (!splitted.empty()) {
                dstIP_ = splitted[0];
                if (splitted.size() > 1 && !splitted[1].empty()) {
                    dstPort_ = std::stoi(splitted[1]);
                } else {
                    dstPort_ = 80;
                }
            } else {
                log_->write("[" + to_string(uuid_) +
                                    "] [HTTP setIPPort] wrong request",
                            Log::Level::DEBUG);
                dstIP_.clear();
                dstPort_ = 80;
            }
            break;
        }

        case HTTP::HttpType::connect: {
            target = boost::lexical_cast<std::string>(parsedHttpRequest_.target());
            splitted = splitString(target, ":");

            if (!splitted.empty()) {
                dstIP_ = splitted[0];
                if (splitted.size() > 1 && !splitted[1].empty()) {
                    dstPort_ = std::stoi(splitted[1]);
                } else {
                    dstPort_ = 443;
                }
            } else {
                dstIP_.clear();
                dstPort_ = 443;
            }
            break;
        }
    }
}

/**
 * @brief Converts the parsed TLS record type to a string.
 *
 * @return String representation of the current TLS type.
 */
const std::string HTTP::tlsTypeToString() const {
    switch (parsedTlsRequest_.type) {
        case TlsTypes::TLSHandshake:
            return "TLSHandshake";
        case TlsTypes::ChangeCipherSpec:
            return "ChangeCipherSpec";
        case TlsTypes::ApplicationData:
            return "ApplicationData";
        default:
            return "UNKNOWN TLSTYPE";
    }
}

/**
 * @brief Converts the current parsed request state into a human-readable string.
 *
 * @details
 * - For TLS traffic, returns TLS type, SNI, body size, and body contents.
 * - For HTTP traffic, returns method, version, target, User-Agent, body size,
 *   and body contents.
 * - For CONNECT traffic, returns method and target only.
 *
 * @return Formatted string representation of the current request state.
 */
const std::string HTTP::toString() const {
    switch (httpType()) {
        case HTTP::HttpType::https:
            return std::string("\n") + "TLS Type : " + tlsTypeToString() + "\n" +
                   "SNI : " + parsedTlsRequest_.sni + "\n" + "Body Size : " +
                   boost::lexical_cast<std::string>(parsedTlsRequest_.body.size()) +
                   "\n" + "Body : " + parsedTlsRequest_.body + "\n";

        case HTTP::HttpType::http:
            return std::string("\n") + "Method : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.method()) +
                   "\n" + "Version : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.version()) +
                   "\n" + "Target : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.target()) +
                   "\n" + "User Agent : " +
                   boost::lexical_cast<std::string>(
                           parsedHttpRequest_["User-Agent"]) +
                   "\n" + "Body Size : " +
                   boost::lexical_cast<std::string>(
                           parsedHttpRequest_.body().size()) +
                   "\n" + "Body : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.body()) + "\n";

        case HTTP::HttpType::connect:
            return std::string("\n") + "Method : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.method()) +
                   "\n" + "Target : " +
                   boost::lexical_cast<std::string>(parsedHttpRequest_.target()) +
                   "\n";

        default:
            return "UNKNOWN HTTPTYPE";
    }
}

/**
 * @brief Converts the parsed HTTP response into a human-readable string.
 *
 * @details
 * - Returns the response base fields, body size, and body contents.
 *
 * @return Formatted string representation of the parsed HTTP response.
 */
const std::string HTTP::restoString() const {
    return std::string("\n") +
           boost::lexical_cast<std::string>(parsedHttpResponse_.base()) + "\n" +
           "Body Size : " +
           boost::lexical_cast<std::string>(parsedHttpResponse_.body().length()) +
           "\n" + "Body : " + parsedHttpResponse_.body() + "\n";
}
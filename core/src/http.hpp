#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/lexical_cast.hpp>
#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "log.hpp"

/**
 * @brief The `HTTP` class provides parsing and generation utilities for HTTP and TLS traffic.
 *
 * @details
 * - Parses HTTP requests and responses from a stream buffer.
 * - Detects whether incoming traffic is plain HTTP, HTTPS-related, or CONNECT tunneling.
 * - Parses selected TLS request data such as message type and SNI.
 * - Generates HTTP request and response strings used by the application.
 * - Tracks destination IP address and port derived from parsed request data.
 * - Integrates with configuration and logging components for runtime behavior and diagnostics.
 *
 * @note
 * - Instances are managed through shared ownership using `std::shared_ptr`.
 * - Objects should be created using the `create` factory method.
 */
class HTTP {
public:
    using pointer = std::shared_ptr<HTTP>;

    /**
     * @brief Factory method to create a new `HTTP` instance.
     *
     * @details
     * - Wraps object creation in a `std::shared_ptr`.
     * - Ensures consistent ownership handling across the application.
     *
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param buffer Reference to the stream buffer containing raw input data.
     * @param uuid Unique session identifier.
     * @return A shared pointer to the created `HTTP` instance.
     */
    static pointer create(const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          boost::asio::streambuf &buffer,
                          boost::uuids::uuid uuid) {
        return pointer(new HTTP(config, log, buffer, uuid));
    }

    /**
     * @brief Enumeration of supported HTTP traffic types.
     *
     * @details
     * - `https`: HTTPS-style traffic or TLS-related traffic.
     * - `http`: Standard HTTP request traffic.
     * - `connect`: HTTP CONNECT tunnel request.
     */
    enum class HttpType {
        https,
        http,
        connect
    };

    /**
     * @brief Enumeration of supported TLS record types.
     */
    enum class TlsTypes {
        TLSHandshake,    ///< TLS handshake record.
        ChangeCipherSpec,///< TLS ChangeCipherSpec record.
        ApplicationData  ///< TLS application data record.
    };

    /**
     * @brief Stores parsed TLS request information.
     *
     * @details
     * - Contains the extracted Server Name Indication (SNI), payload body,
     *   and detected TLS record type.
     */
    struct TlsRequest {
        std::string sni; ///< Parsed Server Name Indication value.
        std::string body;///< Raw or extracted TLS payload body.
        TlsTypes type;   ///< Detected TLS record type.
    };

    /**
     * @brief Copy constructor.
     *
     * @param request Existing `HTTP` object to copy.
     */
    explicit HTTP(const HTTP &request);

    /**
     * @brief Destructor for `HTTP`.
     */
    ~HTTP();

    /**
     * @brief Returns the detected HTTP traffic type.
     *
     * @return Reference to the current `HttpType`.
     */
    inline const HTTP::HttpType &httpType() const { return httpType_; }

    /**
     * @brief Sets the HTTP traffic type.
     *
     * @param httpType New HTTP type value.
     */
    inline void httpType(const HTTP::HttpType &httpType) { httpType_ = httpType; }

    /**
     * @brief Detects the type of traffic present in the buffer.
     *
     * @details
     * - Inspects the current buffer contents.
     * - Determines whether the input is HTTP, HTTPS-related, or CONNECT traffic.
     *
     * @return `true` if the type is successfully detected, otherwise `false`.
     */
    bool detectType();

    /**
     * @brief Parses an HTTP request from the buffer.
     *
     * @details
     * - Populates the internal parsed HTTP request object.
     * - May also derive destination information from the parsed request.
     *
     * @return `true` if parsing succeeds, otherwise `false`.
     */
    bool parseHttp();

    /**
     * @brief Parses an HTTP response from the buffer.
     *
     * @details
     * - Populates the internal parsed HTTP response object.
     *
     * @return `true` if parsing succeeds, otherwise `false`.
     */
    bool parseHttpResp();

    /**
     * @brief Parses a TLS request from the buffer.
     *
     * @details
     * - Extracts selected TLS metadata such as record type and SNI when available.
     *
     * @return `true` if parsing succeeds, otherwise `false`.
     */
    bool parseTls();

    /**
     * @brief Generates an HTTP POST request string with the given body.
     *
     * @param body Body content to include in the request.
     * @return Serialized HTTP POST request string.
     */
    const std::string genHttpPostReqString(const std::string &body) const;

    /**
     * @brief Generates a REST-style HTTP POST request string.
     *
     * @return Serialized REST-style HTTP POST request string.
     */
    const std::string genHttpRestPostReqString() const;

    /**
     * @brief Generates an HTTP `200 OK` response string.
     *
     * @param body Body content to include in the response.
     * @return Serialized HTTP response string.
     */
    const std::string genHttpOkResString(const std::string &body) const;

    /**
     * @brief Returns the parsed HTTP request object.
     *
     * @return Const reference to the parsed HTTP request.
     */
    inline const boost::beast::http::request<boost::beast::http::string_body> &
    parsedHttpRequest() & {
        return parsedHttpRequest_;
    }

    /**
     * @brief Returns the parsed HTTP response object.
     *
     * @return Const reference to the parsed HTTP response.
     */
    inline const boost::beast::http::response<boost::beast::http::string_body> &
    parsedHttpResponse() & {
        return parsedHttpResponse_;
    }

    /**
     * @brief Returns the parsed TLS request information.
     *
     * @return Const reference to the parsed TLS request structure.
     */
    inline const TlsRequest &parsedTlsRequest() & { return parsedTlsRequest_; }

    /**
     * @brief Converts the parsed TLS type to a string.
     *
     * @return String representation of the current TLS record type.
     */
    const std::string tlsTypeToString() const;

    /**
     * @brief Returns the destination IP address.
     *
     * @return Const reference to the destination IP string.
     */
    inline const std::string &dstIP() { return dstIP_; }

    /**
     * @brief Returns the destination port.
     *
     * @return Const reference to the destination port.
     */
    inline const unsigned short &dstPort() { return dstPort_; }

    /**
     * @brief Converts the current HTTP object state to a human-readable string.
     *
     * @return String representation of the parsed or generated HTTP state.
     */
    const std::string toString() const;

    /**
     * @brief Converts REST-related HTTP state to a human-readable string.
     *
     * @return String representation of REST-specific state information.
     */
    const std::string restoString() const;

    std::string chunkHeader_;///< Custom chunk-related header value.

private:
    /**
     * @brief Private constructor for `HTTP`.
     *
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param buffer Reference to the stream buffer used for input parsing.
     * @param uuid Unique session identifier.
     */
    explicit HTTP(const std::shared_ptr<Config> &config,
                  const std::shared_ptr<Log> &log,
                  boost::asio::streambuf &buffer,
                  boost::uuids::uuid uuid);

    /**
     * @brief Sets destination IP address and port from parsed request data.
     *
     * @details
     * - Extracts and stores destination connection information after parsing.
     */
    void setIPPort();

    const std::shared_ptr<Config> &config_;///< Reference to the configuration object.
    const std::shared_ptr<Log> &log_;      ///< Reference to the logging object.
    boost::asio::streambuf &buffer_;       ///< Buffer containing raw HTTP or TLS data.

    boost::beast::http::parser<false, boost::beast::http::string_body> parser_;       ///< HTTP request parser.
    boost::beast::http::request<boost::beast::http::string_body> parsedHttpRequest_;  ///< Parsed HTTP request.
    boost::beast::http::response<boost::beast::http::string_body> parsedHttpResponse_;///< Parsed HTTP response.

    HttpType httpType_;          ///< Detected HTTP traffic type.
    std::string dstIP_;          ///< Parsed destination IP or host.
    unsigned short dstPort_;     ///< Parsed destination port.
    TlsRequest parsedTlsRequest_;///< Parsed TLS request metadata.
    boost::uuids::uuid uuid_;    ///< Unique session identifier.
};
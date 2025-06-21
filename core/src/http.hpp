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
 * @brief The HTTP class provides functionality for handling HTTP and TLS protocol parsing and generation.
 * 
 * This class encapsulates logic to parse and generate HTTP requests and responses, handle TLS requests,
 * and manage associated data like IP addresses, ports, and configuration details. It supports both 
 * HTTP and HTTPS (TLS) connections.
 */
class HTTP {
public:
    using pointer = std::shared_ptr<HTTP>;

    /**
     * @brief Factory method to create a new HTTP object.
     *
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object for recording events and errors.
     * @param buffer Reference to a stream buffer containing data for processing.
     * @param uuid A unique identifier for the session, provided as a Boost UUID.
     * @return Shared pointer to the newly created HTTP object.
     */
    static pointer create(const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          boost::asio::streambuf &buffer,
                          boost::uuids::uuid uuid) {
        return pointer(new HTTP(config, log, buffer, uuid));
    }

    /**
     * @brief Enumeration representing the type of HTTP connection.
     */
    enum class HttpType {
        https,
        http,
        connect
    };

    /**
     * @brief Enumeration representing types of TLS messages.
     */
    enum class TlsTypes {
        TLSHandshake,
        ChangeCipherSpec,
        ApplicationData
    };

    /**
     * @brief Structure to store parsed TLS request data.
     */
    struct TlsRequest {
        std::string sni;
        std::string body;
        TlsTypes type;
    };

    /**
     * @brief Copy constructor for HTTP objects.
     *
     * @param request The HTTP object to copy.
     */
    explicit HTTP(const HTTP &request);

    /**
     * @brief Destructor for the HTTP class.
     */
    ~HTTP();

    /**
     * @brief Getter for the HTTP connection type.
     *
     * @return The type of HTTP connection (http, https, or connect).
     */
    inline const HTTP::HttpType &httpType() const { return httpType_; }

    /**
     * @brief Setter for the HTTP connection type.
     *
     * @param httpType The new type of HTTP connection.
     */
    inline void httpType(const HTTP::HttpType &httpType) { httpType_ = httpType; }

    /**
     * @brief Detects the type of HTTP connection based on the buffer data.
     *
     * @return True if detection is successful, false otherwise.
     */
    bool detectType();

    /**
     * @brief Parses an HTTP request from the buffer.
     *
     * @return True if parsing is successful, false otherwise.
     */
    bool parseHttp();

    /**
     * @brief Parses an HTTP response from the buffer.
     *
     * @return True if parsing is successful, false otherwise.
     */
    bool parseHttpResp();

    /**
     * @brief Parses a TLS request from the buffer.
     *
     * @return True if parsing is successful, false otherwise.
     */
    bool parseTls();

    /**
     * @brief Generates an HTTP POST request string.
     *
     * @param body The body of the HTTP POST request.
     * @return A string representation of the HTTP POST request.
     */
    const std::string genHttpPostReqString(const std::string &body) const;

    /**
     * @brief Generates a REST-style HTTP POST request string.
     *
     * @return A string representation of the REST-style HTTP POST request.
     */
    const std::string genHttpRestPostReqString() const;

    /**
     * @brief Generates an HTTP 200 OK response string.
     *
     * @param body The body of the HTTP response.
     * @return A string representation of the HTTP 200 OK response.
     */
    const std::string genHttpOkResString(const std::string &body) const;

    /**
     * @brief Retrieves the parsed HTTP request.
     *
     * @return Reference to the parsed HTTP request.
     */
    inline const boost::beast::http::request<boost::beast::http::string_body> &
    parsedHttpRequest() & {
        return parsedHttpRequest_;
    }

    /**
     * @brief Retrieves the parsed HTTP response.
     *
     * @return Reference to the parsed HTTP response.
     */
    inline const boost::beast::http::response<boost::beast::http::string_body> &
    parsedHttpResponse() & {
        return parsedHttpResponse_;
    }

    /**
     * @brief Retrieves the parsed TLS request.
     *
     * @return Reference to the parsed TLS request.
     */
    inline const TlsRequest &parsedTlsRequest() & { return parsedTlsRequest_; }

    /**
     * @brief Converts the TLS message type to a string representation.
     *
     * @return String representation of the TLS message type.
     */
    const std::string tlsTypeToString() const;

    /**
     * @brief Getter for the destination IP address.
     *
     * @return Destination IP address as a string.
     */
    inline const std::string &dstIP() { return dstIP_; }

    /**
     * @brief Getter for the destination port.
     *
     * @return Destination port as an unsigned short.
     */
    inline const unsigned short &dstPort() { return dstPort_; }

    /**
     * @brief Converts the HTTP object state to a string representation.
     *
     * @return A string representation of the HTTP object's state.
     */
    const std::string toString() const;

    /**
     * @brief Converts the REST-related HTTP state to a string representation.
     *
     * @return A string representation of the REST-related state.
     */
    const std::string restoString() const;

    std::string chunkHeader_;

private:
    /**
     * @brief Private constructor for the HTTP class.
     *
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param buffer Reference to a stream buffer containing data for processing.
     * @param uuid A unique identifier for the session.
     */
    explicit HTTP(const std::shared_ptr<Config> &config,
                  const std::shared_ptr<Log> &log,
                  boost::asio::streambuf &buffer,
                  boost::uuids::uuid uuid);

    /**
     * @brief Sets the destination IP and port based on the parsed data.
     */
    void setIPPort();

    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::streambuf &buffer_;
    boost::beast::http::parser<false, boost::beast::http::string_body> parser_;
    boost::beast::http::request<boost::beast::http::string_body> parsedHttpRequest_;
    boost::beast::http::response<boost::beast::http::string_body> parsedHttpResponse_;
    HttpType httpType_;
    std::string dstIP_;
    unsigned short dstPort_;
    TlsRequest parsedTlsRequest_;
    boost::uuids::uuid uuid_;
};

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/uuid/uuid.hpp>
#include <memory>
#include <mutex>
#include <string>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"

/**
 * @brief The `TCPClient` class manages TCP and optional TLS communication with remote endpoints.
 *
 * @details
 * - Provides connection management, including DNS resolution and socket handling.
 * - Supports both plain TCP and TLS (SSL) communication.
 * - Handles reading and writing of data using Boost.Asio stream buffers.
 * - Manages connection timeouts and error handling.
 * - Designed to be used with shared ownership (`boost::shared_ptr`).
 *
 * @note
 * - Inherits from `boost::enable_shared_from_this` to allow safe shared pointer usage.
 * - Thread safety is ensured using an internal mutex where required.
 */
class TCPClient : public boost::enable_shared_from_this<TCPClient> {
public:
    using pointer = boost::shared_ptr<TCPClient>;
    using tcp = boost::asio::ip::tcp;
    using ssl_stream = boost::asio::ssl::stream<tcp::socket>;

    /**
     * @brief Factory method to create a new `TCPClient` instance.
     *
     * @param io_context Reference to the Boost.Asio I/O context.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @return Shared pointer to the created `TCPClient`.
     */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log) {
        return pointer(new TCPClient(io_context, config, log));
    }

    /**
     * @brief Returns the underlying TCP socket.
     *
     * @return Reference to the TCP socket.
     */
    tcp::socket &socket();

    /**
     * @brief Returns the TLS (SSL) socket stream.
     *
     * @return Reference to the SSL stream.
     */
    ssl_stream &sslSocket();

    /**
     * @brief Indicates whether TLS is enabled for this client.
     *
     * @return `true` if TLS is enabled, otherwise `false`.
     */
    bool tlsEnabled() const;

    /**
     * @brief Checks whether the underlying socket is open.
     *
     * @return `true` if the socket is open, otherwise `false`.
     */
    bool isOpen() const;

    /**
     * @brief Enables TLS for the client connection.
     *
     * @details
     * - Initializes the SSL context.
     * - Prepares the SSL stream for encrypted communication.
     *
     * @return `true` if initialization succeeds, otherwise `false`.
     */
    bool enableTlsClient();

    /**
     * @brief Performs a TLS handshake with the remote endpoint.
     *
     * @details
     * - Must be called after establishing a TCP connection.
     *
     * @return `true` if the handshake succeeds, otherwise `false`.
     */
    bool doHandshakeClient();

    /**
     * @brief Writes data into the internal write buffer.
     *
     * @param buffer Source buffer to copy into the internal write buffer.
     */
    void writeBuffer(boost::asio::streambuf &buffer);

    /**
     * @brief Returns the internal write buffer (lvalue).
     */
    inline boost::asio::streambuf &writeBuffer() & { return writeBuffer_; }

    /**
     * @brief Returns the internal write buffer (rvalue).
     */
    inline boost::asio::streambuf &&writeBuffer() && {
        return std::move(writeBuffer_);
    }

    /**
     * @brief Returns the internal read buffer (lvalue).
     */
    inline boost::asio::streambuf &readBuffer() & { return readBuffer_; }

    /**
     * @brief Returns the internal read buffer (rvalue).
     */
    inline boost::asio::streambuf &&readBuffer() && { return std::move(readBuffer_); }

    /**
     * @brief Establishes a connection to the specified destination.
     *
     * @param dstIP Destination IP address or hostname.
     * @param dstPort Destination port.
     * @return `true` if the connection succeeds, otherwise `false`.
     */
    bool doConnect(const std::string &dstIP, const unsigned short &dstPort);

    /**
     * @brief Writes data to the remote endpoint.
     *
     * @param buffer Buffer containing the data to send.
     */
    void doWrite(boost::asio::streambuf &buffer);

    /**
     * @brief Reads data from the agent-side connection.
     */
    void doReadAgent();

    /**
     * @brief Reads data from the server-side connection.
     */
    void doReadServer();

    /**
     * @brief Shuts down and closes the socket connection.
     */
    void socketShutdown();

    boost::uuids::uuid uuid_;///< Unique identifier for this client session.
    bool end_;               ///< Indicates whether the connection has completed.

private:
    /**
     * @brief Constructs a `TCPClient` instance.
     *
     * @param io_context Reference to the Boost.Asio I/O context.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     */
    explicit TCPClient(boost::asio::io_context &io_context,
                       const std::shared_ptr<Config> &config,
                       const std::shared_ptr<Log> &log);

    /**
     * @brief Resets the connection timeout timer.
     */
    void resetTimeout();

    /**
     * @brief Cancels the connection timeout timer.
     */
    void cancelTimeout();

    /**
     * @brief Handles timeout events.
     *
     * @param error Boost system error code.
     */
    void onTimeout(const boost::system::error_code &error);

    const std::shared_ptr<Config> &config_;///< Reference to the configuration object.
    const std::shared_ptr<Log> &log_;      ///< Reference to the logging object.
    boost::asio::io_context &io_context_;  ///< Reference to the I/O context.

    tcp::socket socket_;                   ///< Underlying TCP socket.
    boost::asio::ssl::context sslContext_; ///< SSL context for TLS connections.
    std::unique_ptr<ssl_stream> sslSocket_;///< SSL stream wrapper.
    bool tlsEnabled_;                      ///< Indicates whether TLS is active.

    boost::asio::streambuf buffer_;     ///< General-purpose buffer.
    boost::asio::streambuf writeBuffer_;///< Buffer for outgoing data.
    boost::asio::streambuf readBuffer_; ///< Buffer for incoming data.

    tcp::resolver resolver_;           ///< DNS resolver.
    boost::asio::steady_timer timeout_;///< Timeout timer.
    mutable std::mutex mutex_;         ///< Mutex for thread-safe operations.
};
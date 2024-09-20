#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <mutex>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"

/*
 * TCPClient is a class for managing a TCP client connection.
 * It handles connecting to an endpoint, sending data, and receiving data.
 */
class TCPClient : public boost::enable_shared_from_this<TCPClient> {
public:
    // Type alias for shared pointer to TCPClient
    using pointer = boost::shared_ptr<TCPClient>;

    /**
     * @brief Factory method to create a new TCPClient instance.
     *
     * @param io_context The Boost Asio I/O context for asynchronous
     * operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @return Shared pointer to a new TCPClient instance.
     */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log);

    /**
     * @brief Getter for the socket.
     * @return Reference to the TCP socket.
     */
    boost::asio::ip::tcp::socket &getSocket();

    /**
     * @brief Moves data from a given stream buffer to the internal write buffer.
     * @param buffer Stream buffer containing data to be written.
     */
    void writeBuffer(boost::asio::streambuf &buffer);

    /**
     * @brief Accessors for the internal buffers.
     * @return Reference or rvalue reference to the internal write buffer.
     */
    boost::asio::streambuf &getWriteBuffer() &;
    boost::asio::streambuf &&getWriteBuffer() &&;

    /**
     * @brief Accessors for the internal buffers.
     * @return Reference or rvalue reference to the internal read buffer.
     */
    boost::asio::streambuf &getReadBuffer() &;
    boost::asio::streambuf &&getReadBuffer() &&;

    /**
     * @brief Initiates a connection to the specified destination IP and port.
     * @param dstIP Destination IP address as a string.
     * @param dstPort Destination port number.
     */
    bool doConnect(const std::string &dstIP, const unsigned short &dstPort);

    /**
     * Writes data from the internal write buffer to the connected socket.
     *
     * @param buffer - Stream buffer containing data to be written.
     */
    void doWrite(boost::asio::streambuf &buffer);

    /**
     * @brief Reads data from the connected socket into the internal read buffer.
     */
    void doRead();

    /**
     * @brief Setter method for uuid instance.
     * @param uuid The new uuid.
     */
    void setUuid(const boost::uuids::uuid& uuid);

    /**
     * @brief Getter for uuid instance.
     * @return The uuid instance.
     */
    boost::uuids::uuid getUuid() const;

private:
    /**
     * @brief Initializes the TCP client with the given I/O context, configuration,
     * and logging objects.
     *
     * @param io_context The Boost Asio I/O context for asynchronous
     * operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     */
    explicit TCPClient(boost::asio::io_context &io_context,
                    const std::shared_ptr<Config> &config,
                    const std::shared_ptr<Log> &log);

    /**
     * @brief This function is used to disable socket's send operations.
     */
    void socketShutdown();

    /**
     * @brief  If the timeout is enabled, start/reset it.
     */
    void resetTimeout();

    /**
     * @brief Cancel the timeout.
     */
    void cancelTimeout();

    /**
     * @brief Handler in case of a timeout expiration.
     */
    void onTimeout(const boost::system::error_code &error);

private:
    const std::shared_ptr<Config>& config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf writeBuffer_;
    boost::asio::streambuf readBuffer_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::deadline_timer timeout_;
    boost::uuids::uuid uuid_;

    mutable std::mutex mutex_;
};

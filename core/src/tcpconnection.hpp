#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "agenthandler.hpp"
#include "general.hpp"
#include "serverhandler.hpp"

/**
 * @brief TCPConnection class that handles the communication with the TCP client.
 * 
 * This class manages reading and writing data to a TCP socket, handles timeouts,
 * and provides methods for connecting, disconnecting, and reading or writing data.
 */
class TCPConnection : public boost::enable_shared_from_this<TCPConnection> {
public:
    using pointer = boost::shared_ptr<TCPConnection>;

    /**
     * @brief Factory method to create a shared pointer to a TCPConnection instance.
     * 
     * @param io_context The IO context for asynchronous operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param client Shared pointer to the associated TCPClient.
     * @return A shared pointer to the newly created TCPConnection instance.
     */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          TCPClient::pointer client) {
        return pointer(new TCPConnection(io_context, config, log, client));
    }

    /**
     * @brief Returns the TCP socket associated with the TCPConnection.
     * 
     * @return A reference to the TCP socket.
     */
    boost::asio::ip::tcp::socket &socket();

    /**
     * @brief Writes the provided buffer to the TCP connection's write buffer.
     * 
     * @param buffer The buffer containing data to write.
     */
    inline void writeBuffer(boost::asio::streambuf &buffer) {
        moveStreambuf(buffer, writeBuffer_);
    }

    /**
     * @brief Returns a constant reference to the TCP connection's read buffer.
     * 
     * @return A constant reference to the read buffer.
     */
    inline const boost::asio::streambuf &readBuffer() & { return readBuffer_; }

    /**
     * @brief Returns a rvalue reference to the TCP connection's read buffer (after moving it).
     * 
     * @return A rvalue reference to the read buffer.
     */
    inline const boost::asio::streambuf &&readBuffer() && {
        return std::move(readBuffer_);
    }

    /**
     * @brief Returns the unique identifier (UUID) of the TCP connection.
     * 
     * @return The UUID associated with the TCP connection.
     */
    inline boost::uuids::uuid uuid() { return uuid_; }

    /**
     * @brief Starts the TCP connection by initiating the reading process.
     */
    void start();

    /**
     * @brief Initiates the reading process from the TCP connection's socket.
     */
    void doRead();

    /**
     * @brief Handles the completion of a read operation from the TCP connection's socket.
     * 
     * @param error The error code indicating the result of the read operation.
     * @param bytes_transferred The number of bytes read from the socket.
     */
    void handleRead(const boost::system::error_code &error,
                    size_t bytes_transferred);

    /**
     * @brief Reads the remaining data from the TCP connection after the initial read.
     */
    void doReadRest();

    /**
     * @brief Initiates the writing process to the TCP connection's socket.
     * 
     * @param handlerPointer A pointer to the handler for the write operation.
     */
    void doWrite(auto handlerPointer);

    /**
     * @brief Shuts down the TCP connection's socket and closes the connection.
     */
    void socketShutdown();

private:
    /**
     * @brief Constructs a new TCPConnection instance with the provided context, configuration, log, and client.
     * 
     * @param io_context The IO context for asynchronous operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @param client Shared pointer to the associated TCPClient.
     */
    explicit TCPConnection(boost::asio::io_context &io_context,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           TCPClient::pointer client);

    /**
     * @brief Resets the timeout timer for the TCP connection.
     */
    void resetTimeout();

    /**
     * @brief Cancels the timeout timer for the TCP connection.
     */
    void cancelTimeout();

    /**
     * @brief Handles the timeout event for the TCP connection.
     * 
     * @param error The error code indicating the result of the timeout event.
     */
    void onTimeout(const boost::system::error_code &error);

    boost::asio::ip::tcp::socket socket_;
    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::io_context &io_context_;
    TCPClient::pointer client_;
    boost::asio::streambuf readBuffer_, writeBuffer_;
    boost::asio::deadline_timer timeout_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::uuids::uuid uuid_;
    std::mutex mutex_;
    bool end_, connect_;
};

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <mutex>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"

/**
 * @brief Represents a TCP client that handles communication with a server.
 * 
 * Uses Boost.Asio for asynchronous networking operations. Supports connection,
 * read, write, and timeout management.
 */
class TCPClient : public boost::enable_shared_from_this<TCPClient> {
public:
    using pointer = boost::shared_ptr<TCPClient>;

    /**
     * @brief Factory method to create a shared pointer to a TCPClient instance.
     * 
     * @param io_context The IO context for asynchronous operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @return pointer A shared pointer to the newly created TCPClient instance.
     */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log) {
        return pointer(new TCPClient(io_context, config, log));
    }

    /**
     * @brief Gets the socket associated with the TCP client.
     * 
     * @return boost::asio::ip::tcp::socket& The socket used for communication.
     */
    boost::asio::ip::tcp::socket &socket();

    /**
     * @brief Writes data from the given buffer to the socket.
     * 
     * @param buffer The buffer containing data to be written.
     */
    void writeBuffer(boost::asio::streambuf &buffer);
    inline boost::asio::streambuf &writeBuffer() & { return writeBuffer_; }
    inline boost::asio::streambuf &&writeBuffer() && {
        return std::move(writeBuffer_);
    }
    inline boost::asio::streambuf &readBuffer() & { return readBuffer_; }
    inline boost::asio::streambuf &&readBuffer() && {
        return std::move(readBuffer_);
    }

    /**
     * @brief Establishes a connection to the specified server.
     * 
     * @param dstIP The destination server's IP address.
     * @param dstPort The destination server's port.
     * @return bool True if the connection was successful, otherwise false.
     */
    bool doConnect(const std::string &dstIP, const unsigned short &dstPort);

    /**
     * @brief Sends data to the server using the provided buffer.
     * 
     * @param buffer The buffer containing data to be sent.
     */
    void doWrite(boost::asio::streambuf &buffer);

    /**
     * @brief Reads data from the agent (client-side).
     */
    void doReadAgent();

    /**
     * @brief Reads data from the server (server-side).
     */
    void doReadServer();

    /**
     * @brief Handles the reading and processing of data based on the current context.
     */
    void doHandle();

    /**
     * @brief Shuts down the socket gracefully.
     */
    void socketShutdown();
    boost::uuids::uuid uuid_;
    bool end_;

private:
    /**
     * @brief Constructs a `TCPClient` instance.
     * 
     * Initializes the socket, resolver, timeout, and stores references to the
     * configuration and logging objects.
     * 
     * @param io_context The IO context for asynchronous operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     */
    explicit TCPClient(boost::asio::io_context &io_context,
                       const std::shared_ptr<Config> &config,
                       const std::shared_ptr<Log> &log);

    /**
     * @brief Resets the timeout timer to handle long-running operations.
     */
    void resetTimeout();

    /**
     * @brief Cancels the timeout timer.
     */
    void cancelTimeout();

    /**
     * @brief Handles timeout events and performs appropriate actions.
     * 
     * @param error The error code associated with the timeout event.
     */
    void onTimeout(const boost::system::error_code &error);
    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf buffer_, writeBuffer_, readBuffer_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::deadline_timer timeout_;
    mutable std::mutex mutex_;
};

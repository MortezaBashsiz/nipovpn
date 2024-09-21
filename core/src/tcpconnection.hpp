#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "agenthandler.hpp"
#include "general.hpp"
#include "serverhandler.hpp"

/*
 * This class handles a single TCP connection.
 * It is responsible for managing the TCP socket and its associated
 * buffers, and it processes incoming and outgoing data.
 */
class TCPConnection : public boost::enable_shared_from_this<TCPConnection> {
public:
    using pointer = boost::shared_ptr<TCPConnection>;

    /**
     * @brief Factory method to create a new TCPConnection instance.
     *
     * @param io_context The Boost Asio I/O context for asynchronous
     * operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @return Shared pointer to a new TCPConnection instance.
     */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          const TCPClient::pointer &client);

    /**
     * @brief Return socket.
     * @return The socket.
     */
    boost::asio::ip::tcp::socket &getSocket();

    /**
     * @brief Moves the content of the given buffer to the write buffer.
     * @param buffer The buffer that will be moved.
     */
    void writeBuffer(boost::asio::streambuf &buffer);

    /**
     * @brief Return read buffer instance.
     * @return Lvalue reference of read buffer.
     */
    boost::asio::streambuf &getReadBuffer() &;

    /**
     * @brief Return read buffer instance.
     * @return Rvalue const reference of read buffer.
     * @note This method is reference qualified.
     */
    boost::asio::streambuf &&getReadBuffer() &&;

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

    /**
     * @brief Start reading asyncronously from the socket.
     */
    void start();

private:
    /**
     * @brief Initializes the TCP connection.
     *
     * @param io_context The Boost Asio I/O context for asynchronous
     * operations.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     */
    TCPConnection(boost::asio::io_context &io_context,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           const TCPClient::pointer &client);

    /**
     * @brief Initiates an asynchronous write operation
     */
    void doWrite();

    /**
     * @brief Start reading from the socket.
     */
    void doRead();

    /**
     * @brief This function is used to disable socket's send operations.
     */
    void socketShutdown();

    /**
     * @brief Handles the completion of an asynchronous read operation.
     * @param error If error occured in reading process, this variable
     * will contain the error's code.
     * @param bytes_transferred Number of the read bytes.
     */
    void handleRead(const boost::system::error_code &error,
                    size_t bytes_transferred);

    /**
     * @brief If the timeout is enabled, start/restart it.
     */
    void resetTimeout();

    /**
     * @brief Cancel the timeout.
     */
    void cancelTimeout();

    /**
     * @brief Handler in case of a timeout expiration.
     * @param error The error code (if any error occured).
     */
    void onTimeout(const boost::system::error_code &error);

private:
    boost::asio::ip::tcp::socket socket_;
    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::io_context &io_context_;
    const TCPClient::pointer &client_;
    boost::asio::streambuf readBuffer_;
    boost::asio::streambuf writeBuffer_;
    boost::asio::deadline_timer timeout_;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    boost::uuids::uuid uuid_;

    std::mutex mutex_;
};

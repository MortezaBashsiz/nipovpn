#pragma once

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include "tcpconnection.hpp"

/**
 * @brief The `TCPServer` class accepts incoming TCP connections and dispatches them for handling.
 *
 * @details
 * - Owns the listening acceptor socket.
 * - Uses the provided Boost.Asio `io_context` for asynchronous accept operations.
 * - Creates `TCPConnection` instances for accepted clients.
 * - Integrates with configuration and logging components.
 *
 * @note
 * - Instances are managed through shared ownership using `boost::shared_ptr`.
 * - Objects should be created using the `create` factory method.
 */
class TCPServer {
public:
    using pointer = boost::shared_ptr<TCPServer>;

    /**
     * @brief Factory method to create a new `TCPServer` instance.
     *
     * @details
     * - Wraps object construction in a `boost::shared_ptr`.
     * - Ensures consistent ownership and lifetime management.
     *
     * @param io_context Reference to the Boost.Asio I/O context.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     * @return Shared pointer to the created `TCPServer`.
     */
    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log) {
        return pointer(new TCPServer(io_context, config, log));
    }

    /**
     * @brief Starts accepting incoming client connections.
     *
     * @details
     * - Begins an asynchronous accept operation on the listening socket.
     * - Accepted connections are passed to `handleAccept()`.
     */
    void startAccept();

private:
    /**
     * @brief Constructs a `TCPServer` instance.
     *
     * @details
     * - Stores references to the configuration, logging object, and I/O context.
     * - Initializes the listening acceptor socket.
     *
     * @param io_context Reference to the Boost.Asio I/O context.
     * @param config Shared pointer to the configuration object.
     * @param log Shared pointer to the logging object.
     */
    explicit TCPServer(boost::asio::io_context &io_context,
                       const std::shared_ptr<Config> &config,
                       const std::shared_ptr<Log> &log);

    /**
     * @brief Handles completion of an asynchronous accept operation.
     *
     * @details
     * - Receives the newly accepted `TCPConnection`.
     * - Inspects the accept result through the provided error code.
     * - Starts connection processing when accept succeeds.
     *
     * @param connection Shared pointer to the accepted `TCPConnection`.
     * @param error Result of the accept operation.
     */
    void handleAccept(TCPConnection::pointer connection,
                      const boost::system::error_code &error);

    const std::shared_ptr<Config> &config_;  ///< Reference to the configuration object.
    const std::shared_ptr<Log> &log_;        ///< Reference to the logging object.
    boost::asio::io_context &io_context_;    ///< Reference to the I/O context.
    boost::asio::ip::tcp::acceptor acceptor_;///< Listening TCP acceptor.
};
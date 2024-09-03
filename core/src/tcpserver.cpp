#include "tcpserver.hpp"

// Constructor for TCPServer
TCPServer::TCPServer(boost::asio::io_context &io_context,
                     const std::shared_ptr<Config> &config,
                     const std::shared_ptr<Log> &log)
    : config_(config),// Initialize configuration
      log_(log),      // Initialize logging
      client_(TCPClient::create(io_context, config,
                                log)),// Create a TCPClient instance
      io_context_(io_context),        // Initialize the I/O context
      acceptor_(
              io_context,
              boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(
                                                     config->listenIp()),// Listen IP
                                             config->listenPort())),     // Listen port
      strand_(boost::asio::make_strand(io_context))                      // Initialize the strand
{
    startAccept();// Begin accepting incoming connections
}

// Starts an asynchronous accept operation
void TCPServer::startAccept() {
    // Create a new TCP connection
    auto connection = TCPConnection::create(io_context_, config_, log_, client_);

    // Set socket option to allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));

    // Initiate asynchronous accept operation using strand to synchronize
    acceptor_.async_accept(
            connection->socket(),// Socket to be accepted
            boost::asio::bind_executor(
                    strand_, [this, connection](const boost::system::error_code &error) {
                        // Handle the result of the accept operation
                        handleAccept(connection, error);
                    }));
}

// Handles the result of an asynchronous accept operation
void TCPServer::handleAccept(TCPConnection::pointer connection,
                             const boost::system::error_code &error) {
    if (!error) {
        // No error, start the connection
        connection->start();
    } else {
        // Log error details
        log_->write("TCPServer handleAccept] " + std::string(error.message()),
                    Log::Level::ERROR);
    }

    // Continue accepting new connections
    startAccept();
}

#pragma once

#include <boost/beast/core/detail/base64.hpp>
#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"
#include "tcpclient.hpp"

/**
 * @brief This class handles requests when the process is running in agent
 * mode.
 *
 * The AgentHandler processes HTTP requests by encrypting/decrypting data,
 * forwarding requests to a TCP server, and handling responses.
 */
class AgentHandler : private Uncopyable {
public:
    using pointer = std::shared_ptr<AgentHandler>;

    /**
     * @brief Factory method to create an instance of AgentHandler.
     *
     * @param readBuffer The buffer to read incoming data from.
     * @param writeBuffer The buffer to write outgoing data to.
     * @param config Shared configuration object.
     * @param log Shared logging object.
     * @param client Shared pointer to a TCP client.
     * @return pointer A shared pointer to the created AgentHandler instance.
     */
    static pointer create(boost::asio::streambuf &readBuffer,
                          boost::asio::streambuf &writeBuffer,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          const TCPClient::pointer &client,
                          const std::string &clientConnStr,
                          boost::uuids::uuid uuid);

    ~AgentHandler();

    /**
     * @brief Handles the HTTP request.
     *
     * This function is called from the TCPConnection::handleRead function.
     * It processes the request, encrypts or decrypts data as needed,
     * forwards requests to the server, and handles responses.
     */
    void handle();

    /**
     * @brief Gets a reference to the HTTP request object.
     *
     * @return The HTTP request object.
     */
    const HTTP::pointer &getRequest() const &;

    /**
     * @brief Gets a rvalue reference to the HTTP request object.
     *
     * @return The HTTP request object (rvalue).
     */
    const HTTP::pointer &&getRequest() const &&;

private:
    /**
     * @brief Constructs an AgentHandler instance.
     *
     * @param readBuffer The buffer to read incoming data from.
     * @param writeBuffer The buffer to write outgoing data to.
     * @param config Shared configuration object.
     * @param log Shared logging object.
     * @param client Shared pointer to a TCP client.
     */
    AgentHandler(boost::asio::streambuf &readBuffer,
                 boost::asio::streambuf &writeBuffer,
                 const std::shared_ptr<Config> &config,
                 const std::shared_ptr<Log> &log,
                 const TCPClient::pointer &client,
                 const std::string &clientConnStr,
                 boost::uuids::uuid uuid);

private:
    std::shared_ptr<Config> config_;
    std::shared_ptr<Log> log_;
    TCPClient::pointer client_;
    boost::asio::streambuf& readBuffer_;
    boost::asio::streambuf& writeBuffer_;
    HTTP::pointer request_;
    std::string clientConnStr_;

    boost::uuids::uuid uuid_;

    std::mutex mutex_;
};

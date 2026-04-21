#pragma once

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <memory>
#include <mutex>

#include "agenthandler.hpp"
#include "general.hpp"
#include "serverhandler.hpp"

class TCPConnection : public boost::enable_shared_from_this<TCPConnection> {
public:
    using pointer = boost::shared_ptr<TCPConnection>;
    using ssl_stream =
            boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          TCPClient::pointer client) {
        return pointer(new TCPConnection(io_context, config, log, client));
    }

    boost::asio::ip::tcp::socket &socket();

    inline TCPClient::pointer client() { return client_; }

    ssl_stream &tlsSocket();

    bool initTlsServerContext();

    bool doHandshakeServer();

    inline void writeBuffer(boost::asio::streambuf &buffer) {
        moveStreambuf(buffer, writeBuffer_);
    }

    inline const boost::asio::streambuf &readBuffer() & { return readBuffer_; }

    inline const boost::asio::streambuf &&readBuffer() && {
        return std::move(readBuffer_);
    }

    inline boost::uuids::uuid uuid() { return uuid_; }

    void startAgent();

    void startServer();

    void doReadAgent();

    void doReadServer();

    void handleReadAgent(const boost::system::error_code &error,
                         size_t bytes_transferred);

    void handleReadServer(const boost::system::error_code &error,
                          size_t bytes_transferred);

    void socketShutdown();

    void enableTunnelMode();

    void relayClientToRemote();

    void relayRemoteToClient();

private:
    explicit TCPConnection(boost::asio::io_context &io_context,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           TCPClient::pointer client);

    void resetTimeout();

    void cancelTimeout();

    void onTimeout(const boost::system::error_code &error);

    boost::asio::ip::tcp::socket socket_;
    boost::asio::ssl::context sslContext_;
    std::unique_ptr<ssl_stream> tlsSocket_;

    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::io_context &io_context_;
    TCPClient::pointer client_;

    boost::asio::streambuf readBuffer_;
    boost::asio::streambuf writeBuffer_;

    boost::asio::steady_timer timeout_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    boost::uuids::uuid uuid_;
    std::mutex mutex_;

    bool end_;
    bool connect_;
    bool tunnelMode_;

    std::array<char, 16384> downstreamBuf_;
    std::array<char, 16384> upstreamBuf_;
};
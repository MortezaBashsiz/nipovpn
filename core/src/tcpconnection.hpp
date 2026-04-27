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
    using ssl_stream = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

    static pointer create(boost::asio::io_context &ioContext,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          TCPClient::pointer client);

    boost::asio::ip::tcp::socket &socket();
    ssl_stream &tlsSocket();

    TCPClient::pointer client() const { return client_; }

    bool initTlsServerContext();
    bool doHandshakeServer();

    void writeBuffer(boost::asio::streambuf &buffer);
    const boost::asio::streambuf &readBuffer() const & { return readBuffer_; }

    boost::uuids::uuid uuid() const { return uuid_; }

    void startAgent();
    void startServer();

    void doReadAgent();
    void doReadServer();

    void handleReadAgent(const boost::system::error_code &error,
                         std::size_t bytesTransferred);

    void handleReadServer(const boost::system::error_code &error,
                          std::size_t bytesTransferred);

    void enableTunnelMode();

    void relayClientToRemote();
    void relayRemoteToClient();

    void socketShutdown();

private:
    TCPConnection(boost::asio::io_context &ioContext,
                  const std::shared_ptr<Config> &config,
                  const std::shared_ptr<Log> &log,
                  TCPClient::pointer client);

    void resetTimeout();
    void cancelTimeout();
    void onTimeout(const boost::system::error_code &error);

    void clearTunnelBuffers();

private:
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ssl::context sslContext_;
    std::unique_ptr<ssl_stream> tlsSocket_;

    std::shared_ptr<Config> config_;
    std::shared_ptr<Log> log_;
    boost::asio::io_context &ioContext_;
    TCPClient::pointer client_;

    boost::asio::streambuf readBuffer_;
    boost::asio::streambuf writeBuffer_;

    boost::asio::streambuf downstreamHttpBuffer_;
    boost::asio::streambuf upstreamHttpBuffer_;

    boost::asio::steady_timer timeout_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    boost::uuids::uuid uuid_;
    std::mutex mutex_;

    bool end_{false};
    bool connect_{false};
    bool tunnelMode_{false};

    bool tunnelRequestHeaderSent_{false};
    bool tunnelResponseHeaderSent_{false};
    bool tunnelRequestHeaderReceived_{false};
    bool tunnelResponseHeaderReceived_{false};

    std::array<char, 16384> downstreamBuf_{};
    std::array<char, 16384> upstreamBuf_{};
};
#pragma once

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "agenthandler.hpp"
#include "general.hpp"
#include "serverhandler.hpp"

class TCPConnection : public boost::enable_shared_from_this<TCPConnection> {
public:
    using pointer = boost::shared_ptr<TCPConnection>;
    using ssl_stream = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

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
    inline const boost::asio::streambuf &&readBuffer() && { return std::move(readBuffer_); }
    inline boost::uuids::uuid uuid() { return uuid_; }

    void startAgent();
    void startServer();

    void doReadAgent();
    void doReadServer();
    void handleReadAgent(const boost::system::error_code &error, size_t bytes_transferred);
    void handleReadServer(const boost::system::error_code &error, size_t bytes_transferred);

    void socketShutdown();

    void enableDirectTunnelMode();

    void relayClientToServer();
    void relayServerToClient();

    void relayAgentToTarget();
    void relayTargetToAgent();

    inline void asyncWriteToAgentConnection(
            const char *data,
            std::size_t bytes,
            std::function<void(const boost::system::error_code &)> done);

    inline void asyncReadFromAgentConnection(
            std::array<char, 8192> &buffer,
            std::function<void(const boost::system::error_code &, std::size_t)> done);

    inline void asyncWriteToAgentServerConnection(
            const char *data,
            std::size_t bytes,
            std::function<void(const boost::system::error_code &)> done);

    inline void asyncReadFromAgentServerConnection(
            std::array<char, 8192> &buffer,
            std::function<void(const boost::system::error_code &, std::size_t)> done);

private:
    struct HttpUtils {
        static std::string toLowerCopy(std::string s);
        static std::string extractHeaders(const std::string &msg);
        static std::string extractBody(const std::string &msg);
        static bool parseContentLength(const std::string &headers, std::size_t &value);
        static bool isChunked(const std::string &headers);

        static bool readRemainingHttpBody(boost::asio::ip::tcp::socket &stream,
                                          boost::asio::streambuf &buf,
                                          boost::system::error_code &ec);

        static bool readRemainingHttpBody(ssl_stream &stream,
                                          boost::asio::streambuf &buf,
                                          boost::system::error_code &ec);

        static bool readRemainingHttpBodyImpl(
                boost::asio::streambuf &buf,
                boost::system::error_code &ec,
                const std::function<std::size_t(boost::asio::mutable_buffer,
                                                boost::system::error_code &)> &readSome);
    };

    explicit TCPConnection(boost::asio::io_context &io_context,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           TCPClient::pointer client);

    void resetTimeout();
    void cancelTimeout();
    void onTimeout(const boost::system::error_code &error);

    void startTunnelReadFromClient();
    void startTunnelPollServer();
    std::string postTunnelAction(const std::string &action, const std::string &rawBody);
    std::string makeRelayHostHeader() const;
    void closeTunnelSession();

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
    boost::asio::steady_timer pollTimer_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    boost::uuids::uuid uuid_;
    std::mutex mutex_;

    bool end_;
    bool connect_;
    bool tunnelMode_;
    bool tunnelClosed_;
    bool pollInProgress_;

    std::array<char, 8192> downstreamBuf_;
    std::array<char, 8192> upstreamBuf_;
};
#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include "agenthandler.hpp"
#include "general.hpp"
#include "serverhandler.hpp"

class TCPConnection : public boost::enable_shared_from_this<TCPConnection> {
public:
    using pointer =
            boost::shared_ptr<TCPConnection>;// Define a type alias for a shared
                                             // pointer to TCPConnection

    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          const TCPClient::pointer &client) {
        return pointer(new TCPConnection(io_context, config, log, client));
    }

    boost::asio::ip::tcp::socket &socket();

    inline void writeBuffer(boost::asio::streambuf &buffer) {
        moveStreambuf(buffer, writeBuffer_);
    }

    inline const boost::asio::streambuf &readBuffer() & { return readBuffer_; }
    inline const boost::asio::streambuf &&readBuffer() && {
        return std::move(readBuffer_);
    }

    inline boost::uuids::uuid uuid() { return uuid_; }

    void start();
    void doRead();
    void handleRead(const boost::system::error_code &error,
                    size_t bytes_transferred);
    void doWrite();
    void handleWrite(const boost::system::error_code &error);
    void socketShutdown();

private:
    explicit TCPConnection(boost::asio::io_context &io_context,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           const TCPClient::pointer &client);
    void resetTimeout();
    void cancelTimeout();
    void onTimeout(const boost::system::error_code &error);

    boost::asio::ip::tcp::socket socket_;
    const std::shared_ptr<Config> &config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::io_context &io_context_;
    const TCPClient::pointer &client_;
    boost::asio::streambuf readBuffer_,
            writeBuffer_;
    boost::asio::deadline_timer timeout_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::uuids::uuid uuid_;
    std::mutex mutex_;
};

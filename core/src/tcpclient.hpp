#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <mutex>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"

class TCPClient : public boost::enable_shared_from_this<TCPClient> {
public:
    using pointer = boost::shared_ptr<TCPClient>;

    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log) {
        return pointer(new TCPClient(io_context, config, log));
    }

    boost::asio::ip::tcp::socket &socket();

    void writeBuffer(boost::asio::streambuf &buffer);

    inline boost::asio::streambuf &writeBuffer() & { return writeBuffer_; }
    inline boost::asio::streambuf &&writeBuffer() && {
        return std::move(writeBuffer_);
    }

    inline boost::asio::streambuf &readBuffer() & { return readBuffer_; }
    inline boost::asio::streambuf &&readBuffer() && {
        return std::move(readBuffer_);
    }

    bool doConnect(const std::string &dstIP, const unsigned short &dstPort);

    void doWrite(boost::asio::streambuf &buffer);

    void doRead();

    void socketShutdown();

    boost::uuids::uuid uuid_;

private:
    explicit TCPClient(boost::asio::io_context &io_context,
                       const std::shared_ptr<Config> &config,
                       const std::shared_ptr<Log> &log);


    void resetTimeout();

    void cancelTimeout();

    void onTimeout(const boost::system::error_code &error);


    const std::shared_ptr<Config>
            &config_;
    const std::shared_ptr<Log> &log_;
    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf writeBuffer_;
    boost::asio::streambuf readBuffer_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::deadline_timer timeout_;


    mutable std::mutex mutex_;
};

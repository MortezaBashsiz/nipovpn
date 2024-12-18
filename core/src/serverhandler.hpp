#pragma once

#include <memory>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"
#include "tcpclient.hpp"


class ServerHandler : private Uncopyable {
public:
    using pointer = std::shared_ptr<ServerHandler>;


    static pointer create(boost::asio::streambuf &readBuffer,
                          boost::asio::streambuf &writeBuffer,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          const TCPClient::pointer &client,
                          const std::string &clientConnStr,
                          boost::uuids::uuid uuid) {
        return pointer(new ServerHandler(readBuffer, writeBuffer, config, log,
                                         client, clientConnStr, uuid));
    }

    ~ServerHandler();

    void handle();
    void continueRead();

    inline const HTTP::pointer &request() & { return request_; }

    inline const HTTP::pointer &&request() && { return std::move(request_); }
    bool end_, connect_;

private:
    explicit ServerHandler(boost::asio::streambuf &readBuffer,
                           boost::asio::streambuf &writeBuffer,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           const TCPClient::pointer &client,
                           const std::string &clientConnStr,
                           boost::uuids::uuid uuid);

    const std::shared_ptr<Config>
            &config_;
    const std::shared_ptr<Log> &log_;
    const TCPClient::pointer &client_;
    boost::asio::streambuf &readBuffer_,
            &writeBuffer_;
    HTTP::pointer request_;
    const std::string
            &clientConnStr_;
    boost::uuids::uuid uuid_;
    std::mutex mutex_;
};

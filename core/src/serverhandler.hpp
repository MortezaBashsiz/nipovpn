#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

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
                          TCPClient::pointer client,
                          std::string clientConnStr,
                          boost::uuids::uuid uuid) {
        return pointer(new ServerHandler(readBuffer, writeBuffer, config, log, client, clientConnStr, uuid));
    }

    ~ServerHandler();

    void handle();

    inline const HTTP::pointer &request() const { return request_; }

    bool end_;
    bool connect_;

private:
    struct HttpUtils {
        static std::string lowerCopy(std::string s);
        static std::string trimCopy(std::string s);
        static std::string getRawHeader(const std::string &headers, const std::string &name);
        static std::string extractHttpBody(const std::string &msg);
    };

    explicit ServerHandler(boost::asio::streambuf &readBuffer,
                           boost::asio::streambuf &writeBuffer,
                           const std::shared_ptr<Config> &config,
                           const std::shared_ptr<Log> &log,
                           TCPClient::pointer client,
                           std::string clientConnStr,
                           boost::uuids::uuid uuid);

    void makeEncryptedHttpResponse(const std::string &plainBody,
                                   const std::string &status = "200 OK",
                                   const std::string &connection = "close");

    void makePlainHttpResponse(const std::string &body,
                               const std::string &status = "200 OK",
                               const std::string &connection = "close");

    static std::mutex sessionsMutex_;
    static std::unordered_map<std::string, TCPClient::pointer> sessions_;

    std::shared_ptr<Config> config_;
    std::shared_ptr<Log> log_;
    TCPClient::pointer client_;
    boost::asio::streambuf &readBuffer_;
    boost::asio::streambuf &writeBuffer_;
    HTTP::pointer request_;
    std::string clientConnStr_;
    boost::uuids::uuid uuid_;
    std::mutex mutex_;
};
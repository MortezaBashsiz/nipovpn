#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"
#include "tcpclient.hpp"
#include "usage.hpp"

class ServerHandler : private Uncopyable {
public:
    using pointer = std::shared_ptr<ServerHandler>;

    static pointer create(boost::asio::streambuf &readBuffer,
                          boost::asio::streambuf &writeBuffer,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          TCPClient::pointer client,
                          std::string clientConnStr,
                          boost::uuids::uuid uuid,
                          std::string &userHint) {
        return pointer(new ServerHandler(readBuffer, writeBuffer, config, log, client, clientConnStr, uuid, userHint));
    }

    static std::size_t revokeSessions(const std::vector<std::string> &users);

    static void closeAllSessions();

    std::string userName() const { return user_ ? user_->name : std::string(); }

    ~ServerHandler();

    void handle();

    inline const HTTP::pointer &request() const { return request_; }

    bool end_;
    bool connect_;
    bool keepAlive_ = false;

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
                           boost::uuids::uuid uuid,
                           std::string &userHint);

    BoolStr authenticate(const std::string &payload);

    std::string srcTag() const;

    static std::string withoutUnverifiedSignature(const std::string &payload);

    void makePayloadHttpResponse(const std::string &plainBody,
                                 const std::string &status = "200 OK",
                                 const std::string &connection = "close");

    std::string connectionHeader() const;
    bool requestKeepAlive_ = false;

    void makePlainHttpResponse(const std::string &body,
                               const std::string &status = "200 OK",
                               const std::string &connection = "close");

    struct Session {
        TCPClient::pointer client;
        std::string user;
    };

    static std::mutex sessionsMutex_;
    static std::unordered_map<std::string, Session> sessions_;

    std::shared_ptr<Config> config_;
    std::shared_ptr<Log> log_;
    TCPClient::pointer client_;
    boost::asio::streambuf &readBuffer_;
    boost::asio::streambuf &writeBuffer_;
    HTTP::pointer request_;
    std::string clientConnStr_;
    boost::uuids::uuid uuid_;
    std::mutex mutex_;

    std::string &userHint_;
    const User *user_ = nullptr;
    std::shared_ptr<Usage::Counter> usage_;
};
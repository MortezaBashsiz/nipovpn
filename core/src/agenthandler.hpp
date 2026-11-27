#pragma once

#include <boost/beast/core/detail/base64.hpp>
#include <memory>
#include <optional>
#include <string>

#include "config.hpp"
#include "general.hpp"
#include "http.hpp"
#include "log.hpp"
#include "tcpclient.hpp"

class AgentHandler : private Uncopyable {
public:
    using pointer = std::shared_ptr<AgentHandler>;

    static pointer create(boost::asio::streambuf &readBuffer,
                          boost::asio::streambuf &writeBuffer,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          TCPClient::pointer client,
                          std::string clientConnStr,
                          boost::uuids::uuid uuid) {
        return pointer(new AgentHandler(readBuffer, writeBuffer, config, log,
                                        client, clientConnStr, uuid));
    }

    ~AgentHandler();

    void handle();

    inline const HTTP::pointer &request() const { return request_; }

    bool end_;
    bool connect_;

private:
    AgentHandler(boost::asio::streambuf &readBuffer,
                 boost::asio::streambuf &writeBuffer,
                 const std::shared_ptr<Config> &config,
                 const std::shared_ptr<Log> &log,
                 TCPClient::pointer client,
                 std::string clientConnStr,
                 boost::uuids::uuid uuid);

    std::string relayHostHeader(const std::shared_ptr<Config> &config);
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

// Sends one envelope action to the server as a decoy HTTP request on client
// and returns the decrypted response body, or nullopt when the server refused
// it or the exchange failed. keepOpen forces a keep-alive request and leaves
// the connection open (used before switching it to a raw tunnel).
std::optional<std::string> postRelayAction(TCPClient &client,
                                           const std::shared_ptr<Config> &config,
                                           const std::shared_ptr<Log> &log,
                                           const boost::uuids::uuid &uuid,
                                           const std::string &action,
                                           const std::string &rawBody,
                                           bool keepOpen = false);

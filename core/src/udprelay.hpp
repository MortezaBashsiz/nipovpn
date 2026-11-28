#pragma once

#include <array>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.hpp"
#include "general.hpp"
#include "log.hpp"
#include "tcpclient.hpp"
#include "usage.hpp"

namespace udp_relay {
    static constexpr std::size_t MaxDatagram = 65535;
    static constexpr std::size_t FrameHeader = 2;

    std::size_t socksAddressLength(const std::string &data, std::size_t offset);

    std::string encodeSocksAddress(const boost::asio::ip::udp::endpoint &endpoint);

    bool appendFrame(std::string &out, const std::string &body);

    std::vector<std::string> takeFrames(std::string &buffer, bool &ok);

    std::optional<std::string> socksPacketToBody(const std::string &packet);

    inline std::string bodyToSocksPacket(const std::string &body) {
        return std::string(3, '\0') + body;
    }
}// namespace udp_relay

class UdpSession : public std::enable_shared_from_this<UdpSession> {
public:
    using pointer = std::shared_ptr<UdpSession>;
    using Sink = std::function<void(std::string body)>;

    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          boost::uuids::uuid uuid,
                          std::string user,
                          std::shared_ptr<Usage::Counter> usage) {
        return pointer(new UdpSession(io_context, config, log, uuid, std::move(user), std::move(usage)));
    }

    void start(Sink sink = nullptr);

    void send(std::string body);

    std::string drain(std::size_t maxBytes);

    void close();

    bool closed() const { return closed_; }
    bool idle(std::chrono::steady_clock::time_point now) const;
    const std::string &user() const { return user_; }

    static bool add(const std::string &key, const pointer &session);
    static pointer find(const std::string &key, const std::string &user);
    static void remove(const std::string &key, const std::string &user);
    static std::size_t revoke(const std::vector<std::string> &users);
    static void closeAll();

private:
    UdpSession(boost::asio::io_context &io_context,
               const std::shared_ptr<Config> &config,
               const std::shared_ptr<Log> &log,
               boost::uuids::uuid uuid,
               std::string user,
               std::shared_ptr<Usage::Counter> usage);

    using udp = boost::asio::ip::udp;

    struct Socket {
        udp::socket socket;
        udp::endpoint sender;
        std::vector<char> buffer;
        explicit Socket(boost::asio::io_context &io) : socket(io), buffer(udp_relay::MaxDatagram) {}
    };

    void doSend(const std::string &body);
    std::optional<udp::endpoint> resolve(const std::string &body, std::size_t addressLength);
    Socket *socketFor(const udp::endpoint &target);
    void receive(Socket &s);
    void deliver(std::string body);
    void touch();
    void armIdleTimer();

    static void sweepLocked();

    boost::asio::io_context &io_context_;
    std::shared_ptr<Config> config_;
    std::shared_ptr<Log> log_;
    boost::uuids::uuid uuid_;
    std::string user_;
    std::shared_ptr<Usage::Counter> usage_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::asio::steady_timer idleTimer_;

    std::unique_ptr<Socket> v4_;
    std::unique_ptr<Socket> v6_;
    std::unordered_map<std::string, udp::endpoint> resolved_;

    Sink sink_;
    std::mutex queueMutex_;
    std::deque<std::string> queue_;
    std::size_t queuedBytes_ = 0;

    std::atomic<std::int64_t> lastActivity_;
    std::atomic_bool closed_{false};

    static std::mutex registryMutex_;
    static std::unordered_map<std::string, pointer> registry_;
};

class UdpAssociation : public std::enable_shared_from_this<UdpAssociation> {
public:
    using pointer = std::shared_ptr<UdpAssociation>;
    using udp = boost::asio::ip::udp;

    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          TCPClient::pointer client,
                          boost::uuids::uuid uuid,
                          boost::asio::ip::address clientAddress,
                          boost::asio::ip::address bindAddress) {
        return pointer(new UdpAssociation(io_context, config, log, std::move(client), uuid,
                                          clientAddress, bindAddress));
    }

    bool open();

    udp::endpoint localEndpoint() const;

    void start();

    void close();

private:
    UdpAssociation(boost::asio::io_context &io_context,
                   const std::shared_ptr<Config> &config,
                   const std::shared_ptr<Log> &log,
                   TCPClient::pointer client,
                   boost::uuids::uuid uuid,
                   boost::asio::ip::address clientAddress,
                   boost::asio::ip::address bindAddress);

    std::optional<std::string> post(const std::string &action, const std::string &rawBody,
                                    bool keepOpen = false);

    void receiveFromClient();
    std::optional<std::string> acceptClientPacket(std::size_t bytes);
    void sendToClient(const std::string &body);

    void pollServer();

    void readFromServer();
    void writeToServer(std::string frames);
    void writeNext();

    std::string tag() const;

    boost::asio::io_context &io_context_;
    std::shared_ptr<Config> config_;
    std::shared_ptr<Log> log_;
    TCPClient::pointer client_;
    boost::uuids::uuid uuid_;
    boost::asio::ip::address clientAddress_;
    boost::asio::ip::address bindAddress_;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    udp::socket socket_;
    std::optional<udp::endpoint> clientEndpoint_;
    udp::endpoint sender_;
    std::vector<char> recvBuffer_;

    boost::asio::steady_timer pollTimer_;
    unsigned int pollDelayMs_ = 0;
    bool pollInProgress_ = false;

    std::mutex postMutex_;

    std::array<char, BufferSize> serverBuffer_;
    std::string serverPending_;
    std::deque<std::string> writeQueue_;
    std::size_t writeQueuedBytes_ = 0;

    bool tunnel_ = false;
    std::atomic_bool closed_{false};
};

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

/*
 * UDP relay (SOCKS5 UDP ASSOCIATE).
 *
 * A datagram travels between agent and server as a frame:
 *
 *     [u16 big-endian length] [ATYP] [address] [u16 port] [payload]
 *
 * The part after the length is exactly the SOCKS5 UDP request header without
 * its RSV and FRAG bytes, so the agent turns a SOCKS5 UDP packet into a frame
 * by dropping three bytes, and back by adding them. Several frames are simply
 * concatenated: in polling mode a batch is the data of one udpsend / udprecv
 * action, in direct tunnel mode frames flow back to back on the connection.
 */
namespace udp_relay {
    static constexpr std::size_t MaxDatagram = 65535;
    static constexpr std::size_t FrameHeader = 2;

    // Bytes of the SOCKS5 address at data[offset] (ATYP .. port), or 0 when
    // it is malformed or truncated.
    std::size_t socksAddressLength(const std::string &data, std::size_t offset);

    // SOCKS5 address (ATYP .. port) of an endpoint.
    std::string encodeSocksAddress(const boost::asio::ip::udp::endpoint &endpoint);

    // Appends one frame. False when the body does not fit in a frame.
    bool appendFrame(std::string &out, const std::string &body);

    // Removes every complete frame from the front of buffer and returns their
    // bodies; a trailing partial frame stays in buffer. ok is set to false
    // when a frame is malformed.
    std::vector<std::string> takeFrames(std::string &buffer, bool &ok);

    // SOCKS5 UDP packet (RSV RSV FRAG ATYP ...) to frame body; nullopt for
    // fragments and malformed packets.
    std::optional<std::string> socksPacketToBody(const std::string &packet);

    inline std::string bodyToSocksPacket(const std::string &body) {
        return std::string(3, '\0') + body;
    }
}// namespace udp_relay

// Server side of one association: owns the UDP sockets that talk to targets.
class UdpSession : public std::enable_shared_from_this<UdpSession> {
public:
    using pointer = std::shared_ptr<UdpSession>;
    // Receives each datagram from a target as a frame body.
    using Sink = std::function<void(std::string body)>;

    static pointer create(boost::asio::io_context &io_context,
                          const std::shared_ptr<Config> &config,
                          const std::shared_ptr<Log> &log,
                          boost::uuids::uuid uuid,
                          std::string user,
                          std::shared_ptr<Usage::Counter> usage) {
        return pointer(new UdpSession(io_context, config, log, uuid, std::move(user), std::move(usage)));
    }

    // With a sink, replies go to it; without, they are queued for drain().
    void start(Sink sink = nullptr);

    // Sends a frame body (address + payload) to its target.
    void send(std::string body);

    // Queued replies as concatenated frames, at most maxBytes (but always at
    // least one frame when any is queued).
    std::string drain(std::size_t maxBytes);

    void close();

    bool closed() const { return closed_; }
    bool idle(std::chrono::steady_clock::time_point now) const;
    const std::string &user() const { return user_; }

    // Registry of polling-mode sessions, keyed by agent session id.
    // False when key already belongs to another user.
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

// Agent side of one SOCKS5 UDP ASSOCIATE: the UDP socket the local client
// talks to, and the relay of its datagrams to the server.
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

    // Binds the local UDP socket and opens the association on the server.
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
    // Frame body of a datagram from the local client, or nullopt to drop it.
    std::optional<std::string> acceptClientPacket(std::size_t bytes);
    void sendToClient(const std::string &body);

    // Polling mode.
    void pollServer();

    // Direct tunnel mode.
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

#ifndef PROXY_HPP
#define PROXY_HPP

#include "config.hpp"
#include "log.hpp"
#include "request.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio/ssl.hpp>

class Proxy
{
public:
	Proxy(Config config);
	~Proxy();
	Config nipoConfig;
	Log nipoLog;
	std::string send(request request_);
	std::string sendClientHello(std::string data, std::string server, std::string port);
};

struct Client
{
    boost::asio::io_service& io_service;
    boost::asio::ip::tcp::socket socket;

    Client(boost::asio::io_service& svc, std::string const& host, std::string const& port) 
        : io_service(svc), socket(io_service) 
    {
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::iterator endpoint = resolver.resolve(boost::asio::ip::tcp::resolver::query(host, port));
        boost::asio::connect(this->socket, endpoint);
    };

    void send(std::vector<unsigned char> message) {
        socket.send(boost::asio::buffer(message));
    }
};

#endif //PROXY_HPP
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
	std::string send(std::vector<unsigned char> message) {
		auto bytes_transferred = boost::asio::write(socket, boost::asio::buffer(message));

		boost::asio::streambuf read_buffer;
			bytes_transferred = boost::asio::read(socket, read_buffer,
				boost::asio::transfer_exactly(bytes_transferred));
			unsigned char data[bytes_transferred];
		std::memcpy(data, boost::asio::buffer_cast<const void*>(read_buffer.data()), bytes_transferred);
			std::stringstream tempStr;
				tempStr << std::hex << std::setfill('0');
				for (int i = 0; i < sizeof(data); ++i)
				{
					tempStr << std::setw(2) << static_cast<unsigned>(data[i]);
				}
				return tempStr.str();
	}
};

#endif //PROXY_HPP
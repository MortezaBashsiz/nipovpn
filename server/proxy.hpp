#ifndef PROXY_HPP
#define PROXY_HPP

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

struct proxyRequest{
	std::string host;
	std::string port;
	std::string method;
	std::string uri;
	int httpVersion;
	std::string userAgent;
	std::vector<header> headers;
	int contentLength = 0;
	std::string content;
};

class Proxy
{
public:
	Proxy(Config config);
	~Proxy();
	Config nipoConfig;
	Log nipoLog;
	std::string send(proxyRequest request_);
};

#endif //PROXY_HPP
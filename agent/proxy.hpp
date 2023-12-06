#ifndef PROXY_HPP
#define PROXY_HPP

#include "config.hpp"
#include "log.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lexical_cast.hpp>

class Proxy
{
public:
	Proxy(Config config);
	~Proxy();
	Config nipoConfig;
	Log nipoLog;

	bool isClientHello, isChangeCipherSpec;
	std::string request, response, serverName;
	int dataLen;
	
	void send();

};

#endif //PROXY_HPP
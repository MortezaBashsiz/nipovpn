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
#include <boost/lexical_cast.hpp>

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

	std::string toString()
  {
  	std::string allHeaders = "";
  	for (std::size_t i = 0; i < headers.size(); ++i)
		{
			allHeaders += headers[i].name + " : " + headers[i].value + "\n";
		}
    return 	"host: " + host + "\n"
    				+ "port: " + port + "\n"
    				+ "method: " + method + "\n"
    				+ "uri: " + uri + "\n"
						+	"httpVersion: " + std::to_string(httpVersion) + "\n"
						+ "userAgent: " + userAgent + "\n"
						+	allHeaders
						+	"contentLength: " +	std::to_string(contentLength) + "\n"
						+	"content : \n " + content + "\n";
  }
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
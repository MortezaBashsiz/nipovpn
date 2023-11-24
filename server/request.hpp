#ifndef REQUEST_HPP
#define REQUEST_HPP

#include "config.hpp"
#include "log.hpp"
#include "response.hpp"
#include "header.hpp"
#include "encrypt.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>
#include <boost/process/async_pipe.hpp>
#include <boost/lexical_cast.hpp>

struct request{
	boost::beast::http::verb method = boost::beast::http::verb::get;
	std::string host,
							port = "80" ,
							uri,
							clientIP ,
							clientPort ,
							httpVersion ,
							isClientHello ,
							isChangeCipherSpec ,
							userAgent ,
							contentLength = "0" ,
							content; 

	boost::beast::http::request<boost::beast::http::string_body> parsedRequest;
	void parse(std::string request);
	std::string toString()
  {
    return 	std::string("\n#######################################################################\n")
    				+ "method: " + boost::lexical_cast<std::string>(method) + "\n"
    				+ "uri: " + uri + "\n"
						+	"clientIP: " + clientIP + "\n"
						+	"clientPort: " + clientPort + "\n"
						+	"httpVersion: " + httpVersion + "\n"
						+	"isClientHello: " +	isClientHello + "\n"
						+	"isChangeCipherSpec: " + isChangeCipherSpec + "\n"
						+	"contentLength: " +	contentLength + "\n"
						+	"content :\n " + content + "\n"
						+ "#######################################################################\n";
  }
};

bool headersEqual(const std::string& a, const std::string& b);
static bool tolowerCompare(char a, char b);

class RequestHandler {
	public:
		RequestHandler(const RequestHandler&) = delete;
		RequestHandler& operator=(const RequestHandler&) = delete;
		explicit RequestHandler(Config config, const std::string& docRoot);
		void handleRequest(request& req, response& resp);
		Log nipoLog;
		Config nipoConfig;
		Encrypt nipoEncrypt;
	
	private:
		std::string docRoot_;
		static bool urlDecode(const std::string& in, std::string& out);
};

#endif // REQUEST_HPP
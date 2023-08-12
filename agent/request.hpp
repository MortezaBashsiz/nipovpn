#ifndef REQUEST_HPP
#define REQUEST_HPP

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "response.hpp"
#include "encrypt.hpp"
#include "proxy.hpp"

#include <boost/beast/http.hpp>
#include <boost/process/async_pipe.hpp>
#include <boost/lexical_cast.hpp>

struct request{
	std::string host;
	std::string port = "80";
	boost::beast::http::verb method;
	std::string uri;
	std::string clientIP;
	std::string clientPort;
	std::string httpVersion;
	std::string userAgent;
	std::string contentLength = "0";
	std::string content;

	boost::beast::http::request<boost::beast::http::string_body> parsedRequest;
	void parse(std::string request);
	std::string toString()
  {
    return 	"method: " + boost::lexical_cast<std::string>(method) + ", \n"
    				+ "uri: " + uri + ", \n"
						+	"clientIP: " + clientIP + ", \n"
						+	"clientPort: " + clientPort + ", \n"
						+	"httpVersion: " + httpVersion + ", \n"
						+	"contentLength: " +	contentLength + ", \n"
						+	"content : , \n " + content + ", \n";
  }
};

bool headersEqual(const std::string& a, const std::string& b);
static bool tolowerCompare(char a, char b);

class RequestHandler {
	public:
		RequestHandler(const RequestHandler&) = delete;
		RequestHandler& operator=(const RequestHandler&) = delete;
		explicit RequestHandler(Config config);
		void handleRequest(request& req, response& resp);
		Log nipoLog;
		Config nipoConfig;
		Encrypt nipoEncrypt;
		Proxy nipoProxy;
	
	private:
		std::string docRoot_;
		static bool urlDecode(const std::string& in, std::string& out);
};

#endif // REQUEST_HPP
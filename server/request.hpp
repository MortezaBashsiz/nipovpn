#ifndef SERVERREQUEST_HPP
#define SERVERREQUEST_HPP

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "response.hpp"
#include "encrypt.hpp"

struct request{
	std::string method;
	std::string uri;
	std::string clientIP;
	std::string clientPort;
	int httpVersionMajor;
	int httpVersionMinor;
	std::vector<header> headers;
	int contentLength = 0;
	std::string content;

	std::string toString()
  {
  	std::string allHeaders = "";
  	for (std::size_t i = 0; i < headers.size(); ++i)
		{
			allHeaders += headers[i].name + " : " + headers[i].value + ", \n";
		}
    return 	"method: " + method + ", \n"
    				+ "uri: " + uri + ", \n"
						+	"clientIP: " + clientIP + ", \n"
						+	"clientPort: " + clientPort + ", \n"
						+	"httpVersion: " + std::to_string(httpVersionMajor) +"."+ std::to_string(httpVersionMinor) + ", \n"
						+	allHeaders
						+	"contentLength: " +	std::to_string(contentLength) + ", \n"
						+	"content : , \n " + content + ", \n";
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

class RequestParser{
	public:
		RequestParser();
		void reset();
		enum resultType { good, bad, indeterminate };
		template <typename InputIterator>
		std::tuple<resultType, InputIterator> parse(request& req, InputIterator begin, InputIterator end) {
			while (begin != end)
			{
				resultType result = consume(req, *begin++);
				if (result == good || result == bad)
					return std::make_tuple(result, begin);
			}
			return std::make_tuple(indeterminate, begin);
		}
	
	private:
		static std::string content_length_name_;
		resultType consume(request& req, char input);
		static bool isChar(int c);
		static bool isCtl(int c);
		static bool isTspecial(int c);
		static bool isDigit(int c);
		enum state{
			methodStart,
			method,
			uri,
			httpVersionH,
			httpVersionT1,
			httpVersionT2,
			httpVersionP,
			httpVersionSlash,
			httpVersionMajorStart,
			httpVersionMajor,
			httpVersionMinorStart,
			httpVersionMinor,
			expectingNewline1,
			headerLineStart,
			headerLws,
			headerName,
			spaceBeforeHeaderValue,
			headerValue,
			expectingNewline2,
			expectingNewline3,
			content,
			noContent
		} state_;
};

#endif // SERVERREQUEST_HPP
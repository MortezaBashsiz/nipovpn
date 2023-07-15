#ifndef REQUEST_HPP
#define REQUEST_HPP

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "response.hpp"

struct request{
	std::string method;
	std::string uri;
	int httpVersionMajor;
	int httpVersionMinor;
	std::vector<header> headers;
	body requestBody;
};

class requestHandler {
	public:
		requestHandler(const requestHandler&) = delete;
		requestHandler& operator=(const requestHandler&) = delete;
		explicit requestHandler(Config nipoConfig, const std::string& docRoot);
		void handleRequest(const request& req, response& resp);
		Log nipoLog;
	
	private:
	  std::string docRoot_;
	  static bool urlDecode(const std::string& in, std::string& out);
};

class requestParser{
	public:
		requestParser();
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
			expectingNewline3
		} state_;
};

#endif // REQUEST_HPP
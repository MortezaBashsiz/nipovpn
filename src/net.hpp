#ifndef NET_HPP
#define NET_HPP

#include <string>
#include <vector>
#include <set>
#include <asio.hpp>

#include "header.hpp"
#include "config.hpp"
#include "log.hpp"
#include "response.hpp"

using namespace std;

const std::string statusStringOk = "HTTP/1.0 200 OK\r\n";
const std::string statusStringBadRequest = "HTTP/1.0 400 Bad Request\r\n";
const std::string statusStringUnauthorized = "HTTP/1.0 401 Unauthorized\r\n";
const std::string statusStringForbidden = "HTTP/1.0 403 Forbidden\r\n";
const std::string statusStringNotFound = "HTTP/1.0 404 Not Found\r\n";
const std::string statusStringInternalServerError = "HTTP/1.0 500 Internal Server Error\r\n";
const std::string statusStringServiceUnavailable = "HTTP/1.0 503 Service Unavailable\r\n";

const char miscNameValueSeparator[] = { ':', ' ' };
const char miscCrlf[] = { '\r', '\n' };

std::string statusToString(response::statusType status);

struct request{
	string method;
	string uri;
	int httpVersionMajor;
	int httpVersionMinor;
	vector<header> headers;
	body requestBody;
};

struct mimeMapping {
	const char* extension;
	const char* mimeType;
};

std::string mimeExtensionToType(const std::string& extension);

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

class connectionManager;

class connection : public std::enable_shared_from_this<connection> {
	public:
		connection(const connection&) = delete;
		connection& operator=(const connection&) = delete;
		explicit connection(asio::ip::tcp::socket socket, connectionManager& manager, requestHandler& handler);
		void start();
		void stop();
	
	private:
		void doRead();
		void doWrite();
		asio::ip::tcp::socket socket_;
		connectionManager& connectionManager_;
		requestHandler& requestHandler_;
		std::array<char, 8192> buffer_;
		request request_;
		requestParser requestParser_;
		response response_;
};

typedef std::shared_ptr<connection> connectionPtr;

class connectionManager
{
	public:
		connectionManager(const connectionManager&) = delete;
		connectionManager& operator=(const connectionManager&) = delete;
		connectionManager(Config nipoConfig);
		void start(connectionPtr c);
		void stop(connectionPtr c);
		void stopAll();
		Log nipoLog;
	
	private:
		std::set<connectionPtr> connections_;
};

class server {
	public:
		server(const server&) = delete;
		server& operator=(const server&) = delete;
		explicit server(Config nipoConfig);
		void run();
		Log nipoLog;
	
	private:
		void doAccept();
		void doAwaitStop();
		asio::io_context io_context_;
		asio::signal_set signals_;
		asio::ip::tcp::acceptor acceptor_;
		connectionManager connectionManager_;
		requestHandler requestHandler_;
};

#endif // NET_HPP

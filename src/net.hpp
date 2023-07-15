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
#include "request.hpp"

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
asio::const_buffer statusToBuffer(response::statusType status);

struct mimeMapping {
	const char* extension;
	const char* mimeType;
};

std::string mimeExtensionToType(const std::string& extension);

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

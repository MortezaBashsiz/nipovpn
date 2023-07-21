#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <set>

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "response.hpp"
#include "request.hpp"
#include "request.hpp"

class serverConnectionManager;

class serverConnection : public std::enable_shared_from_this<serverConnection> {
	public:
		serverConnection(const serverConnection&) = delete;
		serverConnection& operator=(const serverConnection&) = delete;
		explicit serverConnection(asio::ip::tcp::socket socket, serverConnectionManager& manager, serverRequestHandler& handler);
		void start();
		void stop();
	
	private:
		void doRead();
		void doWrite();
		asio::ip::tcp::socket socket_;
		serverConnectionManager& serverConnectionManager_;
		serverRequestHandler& serverRequestHandler_;
		std::array<char, 8192> buffer_;
		request request_;
		serverRequestParser serverRequestParser_;
		response response_;
};

typedef std::shared_ptr<serverConnection> serverConnectionPtr;

class serverConnectionManager
{
	public:
		serverConnectionManager(const serverConnectionManager&) = delete;
		serverConnectionManager& operator=(const serverConnectionManager&) = delete;
		serverConnectionManager(Config nipoConfig);
		void start(serverConnectionPtr c);
		void stop(serverConnectionPtr c);
		void stopAll();
		Log nipoLog;
	
	private:
		std::set<serverConnectionPtr> connections_;
};

#endif //CONNECTION_HPP
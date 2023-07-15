#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <set>

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "response.hpp"
#include "request.hpp"

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

#endif //CONNECTION_HPP
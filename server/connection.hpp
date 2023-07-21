#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <set>

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "response.hpp"
#include "request.hpp"
#include "request.hpp"

class ConnectionManager;

class Connection : public std::enable_shared_from_this<Connection> {
	public:
		Connection(const Connection&) = delete;
		Connection& operator=(const Connection&) = delete;
		explicit Connection(boost::asio::ip::tcp::socket socket, ConnectionManager& manager, RequestHandler& handler);
		void start();
		void stop();
	
	private:
		void doRead();
		void doWrite();
		boost::asio::ip::tcp::socket socket_;
		ConnectionManager& ConnectionManager_;
		RequestHandler& RequestHandler_;
		std::array<char, 8192> buffer_;
		request request_;
		serverRequestParser serverRequestParser_;
		response response_;
};

typedef std::shared_ptr<Connection> ConnectionPtr;

class ConnectionManager
{
	public:
		ConnectionManager(const ConnectionManager&) = delete;
		ConnectionManager& operator=(const ConnectionManager&) = delete;
		ConnectionManager(Config nipoConfig);
		void start(ConnectionPtr c);
		void stop(ConnectionPtr c);
		void stopAll();
		Log nipoLog;
	
	private:
		std::set<ConnectionPtr> connections_;
};

#endif //CONNECTION_HPP
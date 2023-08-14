#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <set>
#include <boost/beast/http.hpp>
#include <boost/lexical_cast.hpp>

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "response.hpp"
#include "request.hpp"
#include "encrypt.hpp"

class ConnectionManager;

class Connection : public std::enable_shared_from_this<Connection> {
	public:
		Connection(const Connection&) = delete;
		Connection& operator=(const Connection&) = delete;
		explicit Connection(boost::asio::ip::tcp::socket socket, ConnectionManager& manager, RequestHandler& handler, Config config);
		void start();
		void stop();
		Log nipoLog;
		Config nipoConfig;
	
	private:
		void doRead();
		void doWrite();
		boost::asio::ip::tcp::socket socket_;
		ConnectionManager& ConnectionManager_;
		RequestHandler& RequestHandler_;
		std::array<char, 8192> buffer_;
		request request_;
		response response_;
};

typedef std::shared_ptr<Connection> ConnectionPtr;

class ConnectionManager
{
	public:
		ConnectionManager(const ConnectionManager&) = delete;
		ConnectionManager& operator=(const ConnectionManager&) = delete;
		ConnectionManager(Config config);
		void start(ConnectionPtr c);
		void stop(ConnectionPtr c);
		void stopAll();
		Log nipoLog;
		Config nipoConfig;
	
	private:
		std::set<ConnectionPtr> connections_;
};

#endif //CONNECTION_HPP
#ifndef SERVER_HPP
#define SERVER_HPP

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "request.hpp"
#include "response.hpp"
#include "connection.hpp"

using namespace std;

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

#endif // SERVER_HPP

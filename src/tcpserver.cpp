#include "tcpserver.hpp"

/*
* TCPServer
*/
TCPServer::TCPServer(boost::asio::io_context& io_context, 
	const std::shared_ptr<Config>& config, 
	const std::shared_ptr<Log>& log)
	: config_(config),
		log_(log),
		client_(TCPClient::create(io_context, config, log)),
		io_context_(io_context),
		acceptor_(
			io_context,
			boost::asio::ip::tcp::endpoint(
				boost::asio::ip::address::from_string(config->listenIp()),
				config->listenPort()
			)
		)
{
	startAccept();
}

void TCPServer::startAccept()
{
	TCPConnection::pointer newConnection =
		TCPConnection::create(io_context_, config_, log_, client_);
	
	acceptor_.async_accept(newConnection->socket(),
		boost::bind(&TCPServer::handleAccept, 
			this, 
			newConnection,
			boost::asio::placeholders::error)
		);
}

void TCPServer::handleAccept(TCPConnection::pointer newConnection,
	const boost::system::error_code& error)
{
	if (!error)
	{
		boost::asio::socket_base::keep_alive option(true);
		newConnection->socket().set_option(option);
		newConnection->start();
	}
	startAccept();
}
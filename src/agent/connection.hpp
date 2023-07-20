#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <set>

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "response.hpp"
#include "request.hpp"
#include "request.hpp"

class agentConnectionManager;

class agentConnection : public std::enable_shared_from_this<agentConnection> {
	public:
		agentConnection(const agentConnection&) = delete;
		agentConnection& operator=(const agentConnection&) = delete;
		explicit agentConnection(asio::ip::tcp::socket socket, agentConnectionManager& manager, agentRequestHandler& handler);
		void start();
		void stop();
	
	private:
		void doRead();
		void doWrite();
		asio::ip::tcp::socket socket_;
		agentConnectionManager& agentConnectionManager_;
		agentRequestHandler& agentRequestHandler_;
		std::array<char, 8192> buffer_;
		request request_;
		agentRequestParser agentRequestParser_;
		response response_;
};

typedef std::shared_ptr<agentConnection> agentConnectionPtr;

class agentConnectionManager
{
	public:
		agentConnectionManager(const agentConnectionManager&) = delete;
		agentConnectionManager& operator=(const agentConnectionManager&) = delete;
		agentConnectionManager(agentConfig nipoConfig);
		void start(agentConnectionPtr c);
		void stop(agentConnectionPtr c);
		void stopAll();
		Log nipoLog;
	
	private:
		std::set<agentConnectionPtr> connections_;
};

#endif //CONNECTION_HPP
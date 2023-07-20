#ifndef AGENT_HPP
#define AGENT_HPP

#include "config.hpp"
#include "log.hpp"
#include "header.hpp"
#include "request.hpp"
#include "response.hpp"
#include "connection.hpp"

using namespace std;

class agent {
	public:
		agent(const agent&) = delete;
		agent& operator=(const agent&) = delete;
		explicit agent(agentConfig nipoConfig);
		void run();
		Log nipoLog;
	
	private:
		void doAccept();
		void doAwaitStop();
		asio::io_context io_context_;
		asio::signal_set signals_;
		asio::ip::tcp::acceptor acceptor_;
		agentConnectionManager agentConnectionManager_;
		agentRequestHandler agentRequestHandler_;
};

#endif // AGENT_HPP

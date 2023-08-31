#ifndef AGENT_HPP
#define AGENT_HPP

#include "config.hpp"
#include "log.hpp"
#include "session.hpp"

#include <boost/asio.hpp>

class agent {
	public:
		agent(const agent&) = delete;
		agent& operator=(const agent&) = delete;
		explicit agent(Config config);
		void run();
		Config nipoConfig;
		Log nipoLog;
	
	private:
		void doAccept();
		void doAwaitStop();
		boost::asio::io_context io_context_;
		boost::asio::signal_set signals_;
		boost::asio::ip::tcp::acceptor acceptor_;
		SessionManager SessionManager_;
		RequestHandler RequestHandler_;
};

#endif // AGENT_HPP

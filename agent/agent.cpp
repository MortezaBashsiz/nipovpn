#include "agent.hpp"

/*
	Created in: main.cpp
	Implementation of agent based on config
	Here acceptor and sessionManager are defined
	Listening process starts based on the configured IP and Port
*/
agent::agent(Config config)
	: io_context_(1),
		signals_(io_context_),
		acceptor_(io_context_),
		SessionManager_(config),
		RequestHandler_(config),
		nipoLog(config) {

	nipoConfig = config;
	signals_.add(SIGINT);
	signals_.add(SIGTERM);
#if defined(SIGQUIT)
	signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
	nipoLog.write("started in agent mode", nipoLog.levelInfo);
	doAwaitStop();
	/*
		Definition of resolver from boost::asio::io_context io_context_
	*/
	boost::asio::ip::tcp::resolver resolver(io_context_);

	/*
		Definition of endpoint from boost::asio::io_context io_context_
	*/
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(nipoConfig.config.localIp, std::to_string(nipoConfig.config.localPort)).begin();
	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();
	doAccept();
};

/*
	Called from: main.cpp
	Starts the boost::asio::io_context io_context_ which is defined in creation of agent object
*/
void agent::run() {
	io_context_.run();
};

/*
	Called from: agent.cpp agent::agent(Config config)
	Accepts from socket and if get any request will start the sessionManager
*/
void agent::doAccept() {
	acceptor_.async_accept(
			[this](std::error_code ec, boost::asio::ip::tcp::socket socket)
			{
				if (!acceptor_.is_open()) {
					return;
				};
				if (!ec) {
					SessionManager_.start(std::make_shared<Session>(
							std::move(socket), SessionManager_, RequestHandler_, nipoConfig));
				};
				doAccept();
			});
}

/*
	Called from: agent.cpp agent::agent(Config config)
	Closes acceptor and will stop all sessionManager
	It is Mandatory to call doAwaitStop() befor doAccept()
*/
void agent::doAwaitStop() {
	signals_.async_wait(
			[this](std::error_code /*ec*/, int /*signo*/)
			{
				acceptor_.close();
				SessionManager_.stopAll();
			});
}
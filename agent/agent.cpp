#include "agent.hpp"

agent::agent(Config config)
	: io_context_(1),
		signals_(io_context_),
		acceptor_(io_context_),
		ConnectionManager_(config),
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
	boost::asio::ip::tcp::resolver resolver(io_context_);
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(nipoConfig.config.localIp, std::to_string(nipoConfig.config.localPort)).begin();
	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();
	doAccept();
};

void agent::run() {
	io_context_.run();
};

void agent::doAccept() {
	acceptor_.async_accept(
			[this](std::error_code ec, boost::asio::ip::tcp::socket socket)
			{
				if (!acceptor_.is_open()) {
					return;
				};
				if (!ec) {
					ConnectionManager_.start(std::make_shared<Connection>(
							std::move(socket), ConnectionManager_, RequestHandler_, nipoConfig));
				};
				doAccept();
			});
}

void agent::doAwaitStop() {
	signals_.async_wait(
			[this](std::error_code /*ec*/, int /*signo*/)
			{
				acceptor_.close();
				ConnectionManager_.stopAll();
			});
}
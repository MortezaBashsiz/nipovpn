#include "server.hpp"

server::server(Config nipoConfig)
	: io_context_(1),
		signals_(io_context_),
		acceptor_(io_context_),
		ConnectionManager_(nipoConfig),
		RequestHandler_(nipoConfig, nipoConfig.config.webDir),
		nipoLog(nipoConfig) {

	signals_.add(SIGINT);
	signals_.add(SIGTERM);
#if defined(SIGQUIT)
	signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
	nipoLog.write("started in server mode", nipoLog.levelInfo);
	doAwaitStop();
	boost::asio::ip::tcp::resolver resolver(io_context_);
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(nipoConfig.config.ip, std::to_string(nipoConfig.config.port)).begin();
	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();
	doAccept();
};

void server::run() {
	io_context_.run();
};

void server::doAccept() {
	acceptor_.async_accept(
			[this](std::error_code ec, boost::asio::ip::tcp::socket socket)
			{
				if (!acceptor_.is_open()) {
					return;
				};
				if (!ec) {
					ConnectionManager_.start(std::make_shared<Connection>(
							std::move(socket), ConnectionManager_, RequestHandler_));
				};
				doAccept();
			});
}

void server::doAwaitStop() {
	signals_.async_wait(
			[this](std::error_code /*ec*/, int /*signo*/)
			{
				acceptor_.close();
				ConnectionManager_.stopAll();
			});
}
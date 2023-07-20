#include "server.hpp"

server::server(serverConfig nipoConfig)
	: io_context_(1),
		signals_(io_context_),
		acceptor_(io_context_),
		serverConnectionManager_(nipoConfig),
		serverRequestHandler_(nipoConfig, nipoConfig.config.webDir),
		nipoLog(nipoConfig) {

	signals_.add(SIGINT);
	signals_.add(SIGTERM);
#if defined(SIGQUIT)
	signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
	nipoLog.write("started in server mode", nipoLog.levelInfo);
	doAwaitStop();
	asio::ip::tcp::resolver resolver(io_context_);
	asio::ip::tcp::endpoint endpoint = *resolver.resolve(nipoConfig.config.ip, std::to_string(nipoConfig.config.port)).begin();
	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();
	doAccept();
};

void server::run() {
	io_context_.run();
};

void server::doAccept() {
	acceptor_.async_accept(
			[this](std::error_code ec, asio::ip::tcp::socket socket)
			{
				if (!acceptor_.is_open()) {
					return;
				};
				if (!ec) {
					serverConnectionManager_.start(std::make_shared<serverConnection>(
							std::move(socket), serverConnectionManager_, serverRequestHandler_));
				};
				doAccept();
			});
}

void server::doAwaitStop() {
	signals_.async_wait(
			[this](std::error_code /*ec*/, int /*signo*/)
			{
				acceptor_.close();
				serverConnectionManager_.stopAll();
			});
}
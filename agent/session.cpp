#include "session.hpp"

Session::Session(boost::asio::ip::tcp::socket socket,
		SessionManager& manager, RequestHandler& handler, Config config)
	: socket_(std::move(socket)),
		SessionManager_(manager),
		RequestHandler_(handler),
		nipoLog(config),
		nipoEncrypt(config),
		nipoProxy(config)
{
	nipoConfig = config;
	boost::asio::socket_base::keep_alive option(true);
	socket_.set_option(option);
}

void Session::start() {
	doRead();
}

void Session::stop() {
	socket_.close();
}

void Session::doRead() {
	auto self(shared_from_this());
	socket_.async_read_some(boost::asio::buffer(buffer_), [this, self](boost::system::error_code ec, std::size_t bytesTransferred) {
		char data[bytesTransferred];
		std::memcpy(data, buffer_.data(), bytesTransferred);
		std::cout << "PACKET : " << std::endl << data << std::endl;
		if (!ec) {
			request_.parse(data);
			request_.clientIP = socket_.remote_endpoint().address().to_string();
			request_.clientPort = std::to_string(socket_.remote_endpoint().port());
			RequestHandler_.handleRequest(request_, response_);
			doWrite();
		} else if (ec != boost::asio::error::operation_aborted) {
			SessionManager_.stop(shared_from_this());
		}
	});
}

void Session::doWrite() {
	auto self(shared_from_this());
	boost::asio::async_write(socket_, response_.toBuffers(), [this, self](boost::system::error_code ec, std::size_t) {});
	doRead();
}

SessionManager::SessionManager(Config config) : nipoLog(config){
	nipoConfig = config;
}

void SessionManager::start(SessionPtr c) {
	nipoLog.write("started SessionManager", nipoLog.levelDebug);
	sessions_.insert(c);
	c->start();
}

void SessionManager::stop(SessionPtr c) {
	nipoLog.write("stopped SessionManager", nipoLog.levelDebug);
	sessions_.erase(c);
	c->stop();
}

void SessionManager::stopAll() {
	nipoLog.write("stopped All SessionManager", nipoLog.levelDebug);
	for (auto c: sessions_)
		c->stop();
	sessions_.clear();
}
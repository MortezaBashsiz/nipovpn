#include "session.hpp"

Session::Session(boost::asio::ip::tcp::socket socket,
		SessionManager& manager, RequestHandler& handler, Config config)
	: socket_(std::move(socket)),
		SessionManager_(manager),
		RequestHandler_(handler),
		nipoLog(config) 
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
		if (!ec) {
			request_.parse(data);
			request_.clientIP = socket_.remote_endpoint().address().to_string();
			request_.clientPort = std::to_string(socket_.remote_endpoint().port());
			RequestHandler_.handleRequest(request_, response_);
			if (response_.status)
			{
				doWrite();
			} else {
				doRead();
			}
		}
		else if (ec != boost::asio::error::operation_aborted) {
			SessionManager_.stop(shared_from_this());
		}
	});
}

void Session::doWrite() {
	auto self(shared_from_this());
	boost::asio::async_write(socket_, response_.toBuffers(), [this, self](boost::system::error_code ec, std::size_t) {
		if (!ec) {
			boost::system::error_code ignored_ec;
			socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both,
				ignored_ec);
		}
		if (ec != boost::asio::error::operation_aborted) {
			SessionManager_.stop(shared_from_this());
		}
	});
}

SessionManager::SessionManager(Config config) : nipoLog(config){
	nipoConfig = config;
}

void SessionManager::start(SessionPtr session) {
	nipoLog.write("started SessionManager", nipoLog.levelDebug);
	sessions_.insert(session);
	session->start();
}

void SessionManager::stop(SessionPtr session) {
	nipoLog.write("stopped SessionManager", nipoLog.levelDebug);
	sessions_.erase(session);
	session->stop();
}

void SessionManager::stopAll() {
	nipoLog.write("stopped All SessionManager", nipoLog.levelDebug);
	for (auto session: sessions_)
		session->stop();
	sessions_.clear();
}
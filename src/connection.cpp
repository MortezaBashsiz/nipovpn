#include "connection.hpp"

connection::connection(asio::ip::tcp::socket socket,
		connectionManager& manager, serverRequestHandler& handler)
	: socket_(std::move(socket)),
		connectionManager_(manager),
		serverRequestHandler_(handler) {
}

void connection::start() {
	doRead();
}

void connection::stop() {
	socket_.close();
}

void connection::doRead() {
	auto self(shared_from_this());
	socket_.async_read_some(asio::buffer(buffer_), [this, self](std::error_code ec, std::size_t bytesTransferred) {
		request_.clientIP = socket_.remote_endpoint().address().to_string();
		request_.clientPort = std::to_string(socket_.remote_endpoint().port());
		char data[bytesTransferred];
		std::memcpy(data, buffer_.data(), bytesTransferred);
		request_.requestBody.content = data;
		if (!ec) {
			serverRequestParser::resultType result;
			std::tie(result, std::ignore) = serverRequestParser_.parse(
					request_, buffer_.data(), buffer_.data() + bytesTransferred);
			if (result == serverRequestParser::good) {
				serverRequestHandler_.handleRequest(request_, response_);
				doWrite();
			}
			else if (result == serverRequestParser::bad) {
				response_ = response::stockResponse(response::badRequest);
				doWrite();
			}
			else {
				doRead();
			}
		}
		else if (ec != asio::error::operation_aborted) {
			connectionManager_.stop(shared_from_this());
		}
	});
}

void connection::doWrite() {
	auto self(shared_from_this());
	asio::async_write(socket_, response_.toBuffers(), [this, self](std::error_code ec, std::size_t) {
		if (!ec) {
			asio::error_code ignored_ec;
			socket_.shutdown(asio::ip::tcp::socket::shutdown_both,
				ignored_ec);
		}
		if (ec != asio::error::operation_aborted) {
			connectionManager_.stop(shared_from_this());
		}
	});
}

connectionManager::connectionManager(Config nipoConfig) : nipoLog(nipoConfig){
}

void connectionManager::start(connectionPtr c) {
	nipoLog.write("started connectionManager", nipoLog.levelDebug);
	connections_.insert(c);
	c->start();
}

void connectionManager::stop(connectionPtr c) {
	nipoLog.write("stopped connectionManager", nipoLog.levelDebug);
	connections_.erase(c);
	c->stop();
}

void connectionManager::stopAll() {
	nipoLog.write("stopped All connectionManager", nipoLog.levelDebug);
	for (auto c: connections_)
		c->stop();
	connections_.clear();
}
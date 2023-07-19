#include "connection.hpp"

serverConnection::serverConnection(asio::ip::tcp::socket socket,
		serverConnectionManager& manager, serverRequestHandler& handler)
	: socket_(std::move(socket)),
		serverConnectionManager_(manager),
		serverRequestHandler_(handler) {
}

void serverConnection::start() {
	doRead();
}

void serverConnection::stop() {
	socket_.close();
}

void serverConnection::doRead() {
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
			serverConnectionManager_.stop(shared_from_this());
		}
	});
}

void serverConnection::doWrite() {
	auto self(shared_from_this());
	asio::async_write(socket_, response_.toBuffers(), [this, self](std::error_code ec, std::size_t) {
		if (!ec) {
			asio::error_code ignored_ec;
			socket_.shutdown(asio::ip::tcp::socket::shutdown_both,
				ignored_ec);
		}
		if (ec != asio::error::operation_aborted) {
			serverConnectionManager_.stop(shared_from_this());
		}
	});
}

serverConnectionManager::serverConnectionManager(serverConfig nipoConfig) : nipoLog(nipoConfig){
}

void serverConnectionManager::start(serverConnectionPtr c) {
	nipoLog.write("started serverConnectionManager", nipoLog.levelDebug);
	connections_.insert(c);
	c->start();
}

void serverConnectionManager::stop(serverConnectionPtr c) {
	nipoLog.write("stopped serverConnectionManager", nipoLog.levelDebug);
	connections_.erase(c);
	c->stop();
}

void serverConnectionManager::stopAll() {
	nipoLog.write("stopped All serverConnectionManager", nipoLog.levelDebug);
	for (auto c: connections_)
		c->stop();
	connections_.clear();
}

agentConnection::agentConnection(asio::ip::tcp::socket socket,
		agentConnectionManager& manager, agentRequestHandler& handler)
	: socket_(std::move(socket)),
		agentConnectionManager_(manager),
		agentRequestHandler_(handler) {
}

void agentConnection::start() {
	doRead();
}

void agentConnection::stop() {
	socket_.close();
}

void agentConnection::doRead() {
	auto self(shared_from_this());
	socket_.async_read_some(asio::buffer(buffer_), [this, self](std::error_code ec, std::size_t bytesTransferred) {
		request_.clientIP = socket_.remote_endpoint().address().to_string();
		request_.clientPort = std::to_string(socket_.remote_endpoint().port());
		char data[bytesTransferred];
		std::memcpy(data, buffer_.data(), bytesTransferred);
		request_.requestBody.content = data;
		if (!ec) {
			agentRequestParser::resultType result;
			std::tie(result, std::ignore) = agentRequestParser_.parse(
					request_, buffer_.data(), buffer_.data() + bytesTransferred);
			if (result == agentRequestParser::good) {
				agentRequestHandler_.handleRequest(request_, response_);
				doWrite();
			}
			else if (result == agentRequestParser::bad) {
				response_ = response::stockResponse(response::badRequest);
				doWrite();
			}
			else {
				doRead();
			}
		}
		else if (ec != asio::error::operation_aborted) {
			agentConnectionManager_.stop(shared_from_this());
		}
	});
}

void agentConnection::doWrite() {
	auto self(shared_from_this());
	asio::async_write(socket_, response_.toBuffers(), [this, self](std::error_code ec, std::size_t) {
		if (!ec) {
			asio::error_code ignored_ec;
			socket_.shutdown(asio::ip::tcp::socket::shutdown_both,
				ignored_ec);
		}
		if (ec != asio::error::operation_aborted) {
			agentConnectionManager_.stop(shared_from_this());
		}
	});
}

agentConnectionManager::agentConnectionManager(agentConfig nipoConfig) : nipoLog(nipoConfig){
}

void agentConnectionManager::start(agentConnectionPtr c) {
	nipoLog.write("started agentConnectionManager", nipoLog.levelDebug);
	connections_.insert(c);
	c->start();
}

void agentConnectionManager::stop(agentConnectionPtr c) {
	nipoLog.write("stopped agentConnectionManager", nipoLog.levelDebug);
	connections_.erase(c);
	c->stop();
}

void agentConnectionManager::stopAll() {
	nipoLog.write("stopped All agentConnectionManager", nipoLog.levelDebug);
	for (auto c: connections_)
		c->stop();
	connections_.clear();
}
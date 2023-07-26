#include "connection.hpp"
#include "encrypt.hpp"

Connection::Connection(boost::asio::ip::tcp::socket socket,
		ConnectionManager& manager, RequestHandler& handler, Config config)
	: socket_(std::move(socket)),
		ConnectionManager_(manager),
		RequestHandler_(handler),
		nipoLog(config){
	nipoConfig = config;
}

void Connection::start() {
	doRead();
}

void Connection::stop() {
	socket_.close();
}

void Connection::doRead() {
	auto self(shared_from_this());
	socket_.async_read_some(boost::asio::buffer(buffer_), [this, self](boost::system::error_code ec, std::size_t bytesTransferred) {
		char data[bytesTransferred];
		std::memcpy(data, buffer_.data(), bytesTransferred);
		if (!ec) {
			doRead();
			request_.clientIP = socket_.remote_endpoint().address().to_string();
			request_.clientPort = std::to_string(socket_.remote_endpoint().port());
			EVP_CIPHER_CTX* en = EVP_CIPHER_CTX_new();
			EVP_CIPHER_CTX* de = EVP_CIPHER_CTX_new();
			char *plaintext;
			unsigned char *ciphertext;
			unsigned int salt[] = {12345, 54321};
			unsigned char *token;
			int tokenLen, i;
			token = (unsigned char *)nipoConfig.config.token.c_str();
			tokenLen = strlen(nipoConfig.config.token.c_str());
			int len = strlen(data)+1;
			initAes(token, tokenLen, (unsigned char *)&salt, en, de);
			ciphertext = encryptAes(en, (unsigned char *)data, &len);
			plaintext = (char *)decryptAes(de, ciphertext, &len);
			std::cout << "FE : " << std::endl << ciphertext << std::endl;
			std::cout << "FD : " << std::endl << plaintext << std::endl;
			doWrite();
		} else if (ec != boost::asio::error::operation_aborted) {
			ConnectionManager_.stop(shared_from_this());
		}
	});
}

void Connection::doWrite() {
	auto self(shared_from_this());
	boost::asio::async_write(socket_, response_.toBuffers(), [this, self](boost::system::error_code ec, std::size_t) {
		if (!ec) {
			boost::system::error_code ignored_ec;
			socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both,
				ignored_ec);
		}
		if (ec != boost::asio::error::operation_aborted) {
			ConnectionManager_.stop(shared_from_this());
		}
	});
}

ConnectionManager::ConnectionManager(Config config) : nipoLog(config){
	nipoConfig = config;
}

void ConnectionManager::start(ConnectionPtr c) {
	nipoLog.write("started ConnectionManager", nipoLog.levelDebug);
	connections_.insert(c);
	c->start();
}

void ConnectionManager::stop(ConnectionPtr c) {
	nipoLog.write("stopped ConnectionManager", nipoLog.levelDebug);
	connections_.erase(c);
	c->stop();
}

void ConnectionManager::stopAll() {
	nipoLog.write("stopped All ConnectionManager", nipoLog.levelDebug);
	for (auto c: connections_)
		c->stop();
	connections_.clear();
}
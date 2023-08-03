#include "connection.hpp"
#include "encrypt.hpp"

Connection::Connection(boost::asio::ip::tcp::socket socket,
		ConnectionManager& manager, RequestHandler& handler, Config config)
	: socket_(std::move(socket)),
		ConnectionManager_(manager),
		RequestHandler_(handler),
		nipoLog(config),
		nipoEncrypt(config),
		nipoProxy(config)
{
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
			request_.content = data;
			unsigned char *encryptedData;
			int dataLen = strlen(data);
			std::string logMsg = 	"vpn request, " 
														+ request_.clientIP + ":" 
														+ request_.clientPort;
			nipoLog.write(logMsg, nipoLog.levelInfo);
			nipoLog.write("Recieved request from client", nipoLog.levelDebug);
			nipoLog.write(request_.toString(), nipoLog.levelDebug);
			encryptedData = nipoEncrypt.encryptAes(nipoEncrypt.encryptEvp, (unsigned char *)data, &dataLen);
			std::string result = nipoProxy.send((char *)encryptedData);
			nipoLog.write("Response recieved from nipoServer", nipoLog.levelDebug);
			nipoLog.write("\n"+result+"\n", nipoLog.levelDebug);
			response_.parse(result);
			nipoLog.write("Recieved response from niposerver parsed", nipoLog.levelDebug);
			nipoLog.write(response_.toString(), nipoLog.levelDebug);
			int responseContentLength = std::stoi(response_.contentLength);
			char *plainData = (char *)nipoEncrypt.decryptAes(nipoEncrypt.decryptEvp, (unsigned char *)response_.encryptedContent.c_str(), &responseContentLength);
			nipoLog.write("Recieved response from niposerver decrypted", nipoLog.levelDebug);
			response_.content = plainData;
			nipoLog.write("Response sent to client", nipoLog.levelDebug);
			nipoLog.write(response_.toString(), nipoLog.levelDebug);
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
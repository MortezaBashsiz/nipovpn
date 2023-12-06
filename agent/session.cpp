#include "session.hpp"

Session::Session(boost::asio::ip::tcp::socket socket,
		SessionManager& manager, RequestHandler& handler, Config config)
	: socket_(std::move(socket)),
		SessionManager_(manager),
		RequestHandler_(handler),
		nipoLog(config),
		nipoEncrypt(config),
		nipoProxy(config),
		nipoTls(config)
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
		unsigned char data[bytesTransferred];
		std::memcpy(data, buffer_.data(), bytesTransferred);
		if (!ec) {
      std::stringstream tempStr;
      tempStr << std::hex << std::setfill('0');
      for (int i = 0; i < sizeof(data); ++i)
      {
        tempStr << std::setw(2) << static_cast<unsigned>(data[i]);
      }
      nipoTls.requestStr = tempStr.str();
			nipoTls.detectRequestType();
			if (nipoTls.recordHeader.type == "TLSHandshake" || 
					nipoTls.recordHeader.type == "ChangeCipherSpec" || 
					nipoTls.recordHeader.type == "ApplicationData")
			{
				nipoLog.write("Detected TLS Request (" + nipoTls.recordHeader.type + ") from client", nipoLog.levelInfo);
				nipoTls.handle(response_);
				serverName = nipoTls.serverName;
				doWrite(0);
			} else {
				request_.parse(reinterpret_cast<char*>(data));
				request_.clientIP = socket_.remote_endpoint().address().to_string();
				request_.clientPort = std::to_string(socket_.remote_endpoint().port());
				RequestHandler_.handleRequest(request_, response_);
				request_.serverName = serverName ;
				doWrite(1);
			}
		} else if (ec != boost::asio::error::operation_aborted) {
			SessionManager_.stop(shared_from_this());
		}
	});
}

void Session::doWrite(unsigned short mode) {
	auto self(shared_from_this());
	if ( mode == 1 )
		boost::asio::async_write(socket_, response_.toBuffers(), [this, self](boost::system::error_code ec, std::size_t) {});
	else if ( mode == 0 ){
		std::vector<unsigned char> result(response_.content.size() / 2);
		for (std::size_t i = 0; i != response_.content.size() / 2; ++i)
			result[i] = 16 * charToHex(response_.content[2 * i]) + charToHex(response_.content[2 * i + 1]);
		boost::asio::async_write(socket_, boost::asio::buffer(result), [this, self](boost::system::error_code ec, std::size_t) {});
	}
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
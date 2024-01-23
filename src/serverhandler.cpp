#include "serverhandler.hpp"

ServerHandler::ServerHandler(boost::asio::streambuf& readBuffer,
	boost::asio::streambuf& writeBuffer,
	const std::shared_ptr<Config>& config,
	const std::shared_ptr<Log>& log,
	const TCPClient::pointer& client)
	:
		config_(config),
		log_(log),
		client_(client),
		readBuffer_(readBuffer),
		writeBuffer_(writeBuffer)
{	}

ServerHandler::~ServerHandler()
{	}

void ServerHandler::handle()
{
	Request::pointer request_ = Request::create(config_, log_, readBuffer_);
	request_->detectType();
	log_->write("[ServerHandler handle] [Request] : "+request_->toString(), Log::Level::DEBUG);
	client_->doConnect(request_->dstIP(), request_->dstPort());
	client_->writeBuffer(readBuffer_);
	client_->doWrite();
	client_->doRead();
	copyStreamBuff(client_->readBuffer(), writeBuffer_);
}
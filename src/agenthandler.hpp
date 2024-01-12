#ifndef AGENTHADLER_HPP
#define AGENTHADLER_HPP

class TCPClient;

/*
* This class is the handler if the process is running in mode agent
*/
class AgentHandler 
	: private Uncopyable
{
public:
	typedef std::shared_ptr<AgentHandler> pointer;

	static pointer create(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const std::shared_ptr<Config>& config,
		const std::shared_ptr<Log>& log)
	{
		return pointer(new AgentHandler(readBuffer, writeBuffer, config, log));
	}

	~AgentHandler()
	{	}

	/*
	*	This function is handling request. This function is called from TCPConnection::handleRead function(see tcp.hpp)
	* It creates a request object and proceeds to the next steps
	*/
	void handle()
	{
		Request::pointer request_ = Request::create(config_, log_, readBuffer_);
		request_->detectType();
		log_->write("Request detail : "+request_->toString(), Log::Level::DEBUG);
		// boost::asio::io_context io_context_;
		// TCPClient:pointer tcpClient_ = TCPClient::create(io_context_, config_, log_);
		if (request_->httpType() == Request::HttpType::HTTPS)
		{
			copyStreamBuff(readBuffer_, writeBuffer_);
		}
		else
		{
			copyStreamBuff(readBuffer_, writeBuffer_);
		}
	}

private:
	AgentHandler(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const std::shared_ptr<Config>& config,
		const std::shared_ptr<Log>& log)
		:
			config_(config),
			log_(log),
			readBuffer_(readBuffer),
			writeBuffer_(writeBuffer)
	{	}

	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::streambuf &readBuffer_, &writeBuffer_;
};

#endif /* AGENTHADLER_HPP */
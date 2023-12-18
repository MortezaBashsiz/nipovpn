#ifndef AGENTHADLER_HPP
#define AGENTHADLER_HPP

class AgentHandler : private Uncopyable
{
private:
	const Config& config_;
	Log log_;
	boost::asio::streambuf &readBuffer_, &writeBuffer_;

public:
	AgentHandler(boost::asio::streambuf& readBuffer, boost::asio::streambuf& writeBuffer, const Config& config):
	config_(config),
	log_(config),
	readBuffer_(readBuffer),
	writeBuffer_(writeBuffer)
	{	}

	~AgentHandler()
	{	}

	void handle()
	{
		std::iostream os(&writeBuffer_);
		std::string message("222222");
		os << message;
	}
	
};

#endif /* AGENTHADLER_HPP */
#ifndef SERVERHADLER_HPP
#define SERVERHADLER_HPP

class ServerHandler : private Uncopyable
{
private:
	const std::shared_ptr<Config>& config_;
	const std::shared_ptr<Log>& log_;
	boost::asio::streambuf &readBuffer_, &writeBuffer_;

public:
	explicit ServerHandler(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const std::shared_ptr<Config>& config,
		const std::shared_ptr<Log>& log)
		:
			config_(config),
			log_(log),
			readBuffer_(readBuffer),
			writeBuffer_(writeBuffer)
	{	}

	~ServerHandler()
	{	}

	void handle()
	{
		std::iostream os(&writeBuffer_);
		std::string message("OK");
		os << message;
	}

};

#endif /* SERVERHADLER_HPP */
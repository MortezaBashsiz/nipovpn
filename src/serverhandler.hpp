#ifndef SERVERHADLER_HPP
#define SERVERHADLER_HPP

class ServerHandler : private Uncopyable
{
private:
	const Config& config_;
	Log log_;
	boost::asio::streambuf &readBuffer_, &writeBuffer_;

public:
	explicit ServerHandler(boost::asio::streambuf& readBuffer,
		boost::asio::streambuf& writeBuffer,
		const Config& config)
		:
			config_(config),
			log_(config),
			readBuffer_(readBuffer),
			writeBuffer_(writeBuffer)
	{	}

	~ServerHandler()
	{	}

	void handle()
	{
		std::iostream os(&writeBuffer_);
		std::string message("222222");
		os << message;
	}

};

#endif /* SERVERHADLER_HPP */
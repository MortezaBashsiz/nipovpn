#ifndef LOG_HPP
#define LOG_HPP

class Log 
	: private Uncopyable,
		public boost::enable_shared_from_this<Log>
{
public:
	enum Level
	{
		INFO,
		ERROR,
		DEBUG
	};

	typedef std::shared_ptr<Log> pointer;

	static pointer create(const std::shared_ptr<Config>& config)
	{
		return pointer(new Log(config));
	}

	/*
	* Copy constructor if you want to copy and initialize it
	*/
	explicit Log(const std::shared_ptr<Log>& log)
		:
			config_(log->config_),
			logFileClosed(log->logFileClosed),
			level_(log->level_)
	{}

	/*
	* Default distructor
	*/
	~Log(){}

	/*
	* Functions to handle private members
	*/
	const std::shared_ptr<Config>& config() const
	{
		return config_;
	}

	const Level& level() const
	{
		return level_;
	}

	/*
	*	This fucntion returns string of Log::Level
	*/
	const std::string levelToString(const Level& level) const
	{
		std::string result("");
		switch (level){
			case Level::INFO:
				result = "INFO";
				break;
			case Level::ERROR:
				result = "ERROR";
				break;
			case Level::DEBUG:
				result = "DEBUG";
				break;
		}
		return result;
	}

	/*
	*	This function writes message in to the log file
	*/
	const void write(const std::string& message, const Level& level) const
	{
		std::ofstream logFile_(config_->log().file, logFile_.out | logFile_.app);
		if (level <= level_ || level == Level::ERROR){
			if (logFile_.is_open())
			{
				time_t now = time(0);
				auto time = std::time(nullptr);
				auto localtime = *std::localtime(&time);
				std::ostringstream oss;
				oss << std::put_time(&localtime, "%Y-%m-%d_%H:%M:%S");
				auto timeStr = oss.str();
				std::string line = timeStr + " [" + levelToString(level) + "] " + message + "\n";
				logFile_ << line;
				std::cout << line;
				logFile_.close();
			} else
			{
				std::cerr << "Error openning log file : " << config_->log().file  << " make sure the directory and file exists " << "\n";
			}
		}
	}

private:

	/*
	* Default constructor for Config. The main Config object is initialized here
	*/
	explicit Log(const std::shared_ptr<Config>& config)
		:
			config_(config)
	{
		std::ofstream logFile_(config_->log().file, logFile_.out | logFile_.app);
		/*
		* Checks if the log file is there or not
		*/
		if (! logFile_.is_open())
			std::cerr << "Error openning log file : " << config_->log().file  << " make sure the directory and file exists " << "\n";

		/*
		* Checks and validates the level defined in the config.yaml
		* Also initialize the Log::Level
		*/
		if (config_->log().level == std::string("INFO"))
			level_ = Level::INFO;
		else if (config_->log().level == std::string("DEBUG"))
			level_ = Level::DEBUG;
		else
			std::cerr << "Invalid log level : " << config_->log().level  << ", It should be one of [INFO|DEBUG] " << "\n";
		logFile_.close();
	}

	const std::shared_ptr<Config>& config_;
	bool logFileClosed;
	Level level_ = Level::INFO;

};

#endif /* LOG_HPP */
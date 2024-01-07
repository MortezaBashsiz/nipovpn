#ifndef CONFIG_HPP
#define CONFIG_HPP

/*
* This enum defines the modes which program are able to start
* Mode server and agent
*/
enum RunMode
{
	server,
	agent
};

/*
* Class Config will contain all the configuration directives
* This class is declared and initialized in main.cpp
*/
class Config : private Uncopyable
{
private:

	struct Log
	{
		std::string level;
		std::string file;
	};

	struct Server
	{
		std::string listenIp;
		unsigned short listenPort;
		std::string webDir;
		std::string sslKey;
		std::string sslCert;
	};

	struct Agent
	{
		std::string listenIp;
		unsigned short listenPort;
		std::string serverIp;
		unsigned short serverPort;
		std::string encryption;
		std::string token;
		std::string endPoint;
		std::string httpVersion;
		std::string userAgent;
	};


	/*
	* RunMode is declared in general.hpp
	*/
	const RunMode& runMode_;

	/*
	* Configuration file path string
	*/
	std::string filePath_;

	/*
	* It contains the main yaml node of the config file
	* For more information see Config default constructor and also in general.cpp function validateConfig
	*/
	const YAML::Node& configYaml_;

	/*
	* Objects for separation of configuration sections (log, agent, server)
	* For more information see the private items
	*/
	const Log log_;
	const Server server_;
	const Agent agent_;
	std::string listenIp_;
	unsigned short listenPort_;

public:

	/*
	* Default constructor for Config. The main Config object is initialized here
	*/
	explicit Config(const RunMode& mode,
		const std::string& filePath)
		:
			runMode_(mode),
			filePath_(filePath),
			configYaml_(YAML::LoadFile(filePath)),
			log_
			{
				configYaml_["log"]["logLevel"].as<std::string>(),
				configYaml_["log"]["logFile"].as<std::string>()
			},
			server_
			{
				configYaml_["server"]["listenIp"].as<std::string>(),
				configYaml_["server"]["listenPort"].as<unsigned short>(),
				configYaml_["server"]["webDir"].as<std::string>(),
				configYaml_["server"]["sslKey"].as<std::string>(),
				configYaml_["server"]["sslCert"].as<std::string>()
			},
			agent_
			{
				configYaml_["agent"]["listenIp"].as<std::string>(),
				configYaml_["agent"]["listenPort"].as<unsigned short>(),
				configYaml_["agent"]["serverIp"].as<std::string>(),
				configYaml_["agent"]["serverPort"].as<unsigned short>(),
				configYaml_["agent"]["encryption"].as<std::string>(),
				configYaml_["agent"]["token"].as<std::string>(),
				configYaml_["agent"]["endPoint"].as<std::string>(),
				configYaml_["agent"]["httpVersion"].as<std::string>(),
				configYaml_["agent"]["userAgent"].as<std::string>()
			},
			listenIp_("127.0.0.1"),
			listenPort_(0)
	{
		switch (runMode_){
		case RunMode::server:
			listenIp_ = server_.listenIp;
			listenPort_ = server_.listenPort;
			break;
		case RunMode::agent:
			listenIp_ = agent_.listenIp;
			listenPort_ = agent_.listenPort;
			break;
		}
	}

	/*
	* Copy constructor if you want to copy and initialize it
	*/
	explicit Config(const Config& config)
		:
			runMode_(config.runMode()),
			log_(config.log()),
			server_(config.server()),
			agent_(config.agent()),
			configYaml_(YAML::LoadFile(config.filePath()))
	{	}

	/*
	* Default distructor
	*/
	~Config(){}

	/*
	* Functions to handle private members
	*/
	const Log& log() const
	{
		return log_;
	}

	const Server& server() const
	{
		return server_;
	}

	const Agent& agent() const
	{
		return agent_;
	}

	const std::string& listenIp() const
	{
		return listenIp_;
	}

	void listenIp(std::string ip)
	{
		listenIp_ = ip;
	}

	const unsigned short& listenPort() const
	{
		return listenPort_;
	}

	void listenPort(unsigned short port)
	{
		listenPort_ = port;
	}

	const RunMode& runMode() const
	{
		return runMode_;
	}

	const std::string& filePath() const
	{
		return filePath_;
	}

	const std::string modeToString() const
	{
		switch (runMode_){
			case RunMode::server:
				return "server";
				break;
			case RunMode::agent:
				return "agent";
				break;
			default:
				return "UNKNOWN MODE";
		}
	}

	const std::string toString() const{
		return std::string("\nConfig :\n")
		+ "	Log :\n"
		+ "		logLevel: " + log_.level + "\n"
		+ "		logFile: " + log_.file + "\n"
		+ "	server :\n"
		+ "		listenIp: " + server_.listenIp + "\n"
		+ "		listenPort: " + std::to_string(server_.listenPort) + "\n"
		+ "		webDir: " + server_.webDir + "\n"
		+ "		sslKey: " + server_.sslKey + "\n"
		+ "		sslCert: " + server_.sslCert + "\n"
		+ "	agent :\n"
		+ "		listenIp: " + agent_.listenIp + "\n"
		+ "		listenPort: " + std::to_string(agent_.listenPort) + "\n"
		+ "		serverIp: " + agent_.serverIp + "\n"
		+ "		serverPort: " + std::to_string(agent_.serverPort) + "\n"
		+ "		encryption: " + agent_.encryption + "\n"
		+ "		token: " + agent_.token + "\n"
		+ "		endPoint: " + agent_.endPoint + "\n"
		+ "		httpVersion: " + agent_.httpVersion + "\n"
		+ "		userAgent: " + agent_.userAgent + "\n";
	}

};

#endif /* CONFIG_HPP */
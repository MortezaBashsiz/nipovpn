#include "config.hpp"
#include "general.hpp"
#include "log.hpp"
#include "agent.hpp"

int main(int argc=0, char* argv[]=0){

	/*
		Definition: general.hpp
		Read args and validate main arguments
		Acceptable arguments only one config file
		Example :
						sudo ./nipoagent files/config/nipoAgentConfig.json
	*/
	mainArgs mainArgs = validateMainArgs(argc, argv);

	/*
		Definition: config.hpp
		Create main config object and load config from config file
	*/
	Config nipoConfig(mainArgs.configPath);

	/*
		Definition: log.hpp
		Create main log object
	*/
	Log nipoLog(nipoConfig);
	nipoLog.write("starting in agent mode", nipoLog.levelInfo);


	try {
		/*
			Definition: agent.hpp
			Create main agent object and run it
			Here all the sockets and sessions will be defined and started
		*/
		agent nipoAgent(nipoConfig);
		nipoAgent.run();
	}
	catch (std::exception& e) {
		nipoLog.write(e.what(), nipoLog.levelError);
		std::cerr << "exception: " << e.what() << "\n";
		exit(1);
	}
	return 0;
};
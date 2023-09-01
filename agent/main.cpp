#include "config.hpp"
#include "general.hpp"
#include "log.hpp"
#include "agent.hpp"

int main(int argc=0, char* argv[]=0){

	mainArgs mainArgs = validateMainArgs(argc, argv);
	Config nipoConfig(mainArgs.configPath);
	Log nipoLog(nipoConfig);
	nipoLog.write("starting in agent mode", nipoLog.levelInfo);
	try {
		agent nipoAgent(nipoConfig);
		// start agent
	}
	catch (std::exception& e) {
		nipoLog.write(e.what(), nipoLog.levelError);
		std::cerr << "exception: " << e.what() << "\n";
		exit(1);
	}
	return 0;
};
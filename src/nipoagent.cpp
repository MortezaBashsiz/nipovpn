#include "config.hpp"
#include "log.hpp"
#include "agent.hpp"

int main(int argc=0, char* argv[]=0){
	mainArgs mainArgs = validateMainArgs(argc, argv);
	
	if ( mainArgs.runMode == "agent" ){
		agentConfig nipoConfig(mainArgs.configPath);
		Log nipoLog(nipoConfig);
		nipoLog.write("starting in agent mode", nipoLog.levelInfo);
		try {
			agent nipoServer(nipoConfig);
			nipoServer.run();
		}
		catch (std::exception& e) {
			nipoLog.write(e.what(), nipoLog.levelError);
			std::cerr << "exception: " << e.what() << "\n";
			exit(1);
		}
	}
	return 0;
};
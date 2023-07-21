#include "config.hpp"
#include "general.hpp"
#include "log.hpp"
#include "server.hpp"

int main(int argc=0, char* argv[]=0){

	mainArgs mainArgs = validateMainArgs(argc, argv);
	Config nipoConfig(mainArgs.configPath);
	Log nipoLog(nipoConfig);
	nipoLog.write("starting in server mode", nipoLog.levelInfo);
	
	return 0;
};
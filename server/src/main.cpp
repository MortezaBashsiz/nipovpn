#include "main.hpp"

int main(int argc=0, char* argv[]={0}){
	mainArgs mainArgs = validateMainArgs(argc, argv);
	Config nipoConfig(mainArgs.configPath);
	Log nipoLog(nipoConfig.config.logFile, nipoConfig.config.logLevel);

	bool result = nipoLog.write("bbbb", 1, nipoLog.type.warn);
};
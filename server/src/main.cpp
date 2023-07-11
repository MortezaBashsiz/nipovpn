#include "main.h"

int main(int argc=0, char* argv[]={0}){
	validateMainArgs(argc, argv);
	string configPath = argv[1];
	Config nipoConfig(configPath);
	Log nipoLog(nipoConfig.config.logFile, nipoConfig.config.logLevel);

	// bool result = nipoLog.write("bbbb", 1, nipoLog.type.warn);
};
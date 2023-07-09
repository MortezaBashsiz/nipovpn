#include "main.h"

int main(){
	Config nipoConfig("/home/morteza/data/git/MortezaBashsiz/nipovpn/files/config/nipoServerConfig.json");
	Log nipoLog(nipoConfig.config.logFile, nipoConfig.config.logLevel);

	// bool result = nipoLog.write("bbbb", 1, nipoLog.type.warn);
};
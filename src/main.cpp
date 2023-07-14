#include "config.hpp"
#include "log.hpp"
#include "encrypt.hpp"
#include "net.hpp"

int main(int argc=0, char* argv[]=0){
	mainArgs mainArgs = validateMainArgs(argc, argv);
	Config nipoConfig(mainArgs.configPath);
	Log nipoLog(nipoConfig.config.logFile, nipoConfig.config.logLevel);
	
	if ( mainArgs.runMode == "server" ){
		nipoLog.write("nipovpn starting in server mode", nipoLog.levelInfo);
		try {
			server nipoServer(nipoConfig.config.ip, std::to_string(nipoConfig.config.port), nipoConfig.config.webDir);
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
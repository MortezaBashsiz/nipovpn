#include "config.hpp"
#include "log.hpp"
#include "encrypt.hpp"
#include "net.hpp"

int main(int argc=0, char* argv[]=0){
	mainArgs mainArgs = validateMainArgs(argc, argv);
	Config nipoConfig(mainArgs.configPath);
	Log nipoLog(nipoConfig.config.logFile, nipoConfig.config.logLevel);
	
	if ( mainArgs.runMode == "server" ){
		try {
  	  server s(nipoConfig.config.ip, std::to_string(nipoConfig.config.port), nipoConfig.config.webDir);
  	  s.run();
  	}
  	catch (std::exception& e) {
  	  std::cerr << "exception: " << e.what() << "\n";
  	}
	}
  return 0;
};
#include <fstream>
#include <iostream>
#include <json/value.h>
#include <json/json.h>

#include "../env.h"
#include "../general.h"

using namespace std;

class Config{
	private:
		struct userStruct{
			string token;
			string srcIp;
			string endpoint;
		};
		struct configStruct{
			string ip;
			int port;
			string sslKey;
			string sslCert;
			int logLevel;
			string logFile;
			int threads;
			userStruct users[CONFIG_MAX_USER_COUNT]{
				"a30929e7-284a-448c-a7f1-e91b986cd8f5",
				"0.0.0.0",
				"api01"
			};
		};
	public:
		configStruct config;
		Config();
		Config(string file);
		returnMsgCode validate();
		~Config();
};


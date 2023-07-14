#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <fstream>
#include <iostream>
#include <json/value.h>
#include <json/json.h>

#include "general.hpp"

using namespace std;

#define CONFIG_MAX_USER_COUNT 32 
#define CONFIG_IP "0.0.0.0" 
#define CONFIG_PORT 443 
#define CONFIG_WEB_DIR "/var/lib/nipvn" 
#define CONFIG_SSL_KEY "/etc/nipovpn/ssl.key" 
#define CONFIG_SSL_CERT "/etc/nipovpn/ssl.cert" 
#define CONFIG_LOG_LEVEL 1 
#define CONFIG_LOG_FILE "/var/log/niposerver/nipo.log" 
#define CONFIG_THREADS 1 

class Config{
	private:
		struct userStruct{
			string encryption;
			string token;
			string srcIp;
			string endpoint;
		};
		struct configStruct{
			string ip;
			int port;
			string webDir;
			string sslKey;
			string sslCert;
			int logLevel;
			string logFile;
			int threads;
			userStruct users[CONFIG_MAX_USER_COUNT]{
				"AES_256"
				"Bah9aefai8ShoowieXilaij9Eibee6Uax0ief8dooze2yeesa7fu7Kaexohlaz2Kaghas8kai0ro6uoshaofuYae9eorohZ3AipheixaBaif2ughaithoahuat5bai2peja3ul6aeyoothuur0Kah8Ujeede2wai4vahb9saceiceiGh7Ohke5wiex9abaisieGhoayie7ushie9ahM5pheimaevahchixooBoojuB4eediel5aemus1noopheeZ",
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

#endif // CONFIG_HPP
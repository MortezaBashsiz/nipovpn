#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <fstream>
#include <iostream>
#include <json/value.h>
#include <json/json.h>

#include "env.hpp"
#include "general.hpp"

using namespace std;

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
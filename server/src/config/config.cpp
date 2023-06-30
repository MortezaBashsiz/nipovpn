
#include config.h

class Config
{
	private:
		struct user
		{
			char* token;
			char[16] srcIp;
			char* endpoint;
		};
		char[16] ip;
		int port
		char* sslKey;
		char* sslCert;
		int logLevel;
		int threads;
		user* users;

	public:
		Config(string file)
		{
			Json::Value config;
    		std::ifstream configFile("file", std::ifstream::binary);
    		configFile >> config;
    		return config;
		}

		~Config(){}

		Config* get(string file)
		{
			return new Config(file);
		}
};


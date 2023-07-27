#ifndef PROXY_HPP
#define PROXY_HPP

#include "config.hpp"
#include "log.hpp"

class Proxy
{
public:
	Proxy(Config config);
	~Proxy();
	Config nipoConfig;
	Log nipoLog;
	std::string send(std::string encryptedBody, unsigned short plainDataLen);
};

#endif //PROXY_HPP
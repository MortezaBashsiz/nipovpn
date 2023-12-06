#include "tls.hpp"

Tls::Tls(Config config): nipoLog(config), nipoEncrypt(config){
	nipoConfig = config;
}

Tls::~Tls(){
}

void Tls::handle(){
	result = send();
}

std::string Tls::send(){
	Proxy proxy(nipoConfig);
	nipoLog.write("Sending TLS request to originserver : " + toString(), nipoLog.levelDebug);
	std::string result = proxy.sendTLS(data, serverName, port);
	return result;
}
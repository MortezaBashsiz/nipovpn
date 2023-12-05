#ifndef TLS_HPP
#define TLS_HPP

#include "config.hpp"
#include "log.hpp"
#include "proxy.hpp"
#include "encrypt.hpp"

struct RecordHeader{
	std::string type,
							version;
	unsigned short int 	contentLength;				
};

struct HandshakeHeader{
	std::string messageType;
	unsigned short int 	messageLength;
};

struct ClientHello{
	std::string clientVersion,
							clientRandomValue,
							sessionID,
							serverName;
	unsigned short int 	sessionIDLength,
											cipherSuitesLength,
											compressionMethodLength,
											extentionsLength,
											serverNameLength;
};

class Tls{
public:

	Tls(Config config);
	~Tls();
	Config nipoConfig;
	Log nipoLog;
	Encrypt nipoEncrypt;
	
	std::string serverName, port;
	std::string data, result;

	void handle();
	std::string send();

	std::string toString()
	{
		return 	std::string("\n#######################################################################\n")
						+ "data : " + data + " \n"
						+ "serverName : " + serverName + " \n"
						+ "port : " + port + " \n"
						+ "#######################################################################\n";
	}
};

#endif // TLS_HPP
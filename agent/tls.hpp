#ifndef TLS_HPP
#define TLS_HPP

#include "config.hpp"
#include "log.hpp"

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

	RecordHeader recordHeader;
	HandshakeHeader handshakeHeader;
	ClientHello clientHello;
	
	unsigned short port;
	std::string data;

	void handle();
	void parseRecordHeader();
	void parseHandshakeHeader();
	void parseClientHello();
	std::string send();

	std::string toString()
	{
		return 	std::string("\n#######################################################################\n")
						+ "type : " + recordHeader.type + " \n"
						+ "version : " + recordHeader.version + " \n"
						+ "contentLength : " + std::to_string(recordHeader.contentLength) + " \n"
						+ "messageType : " + handshakeHeader.messageType + " \n"
						+ "messageLength : " + std::to_string(handshakeHeader.messageLength) + " \n"
						+ "clientVersion : " + clientHello.clientVersion + " \n"
						+ "clientRandomValue : " + clientHello.clientRandomValue + " \n"
						+ "sessionIDLength : " + std::to_string(clientHello.sessionIDLength) + " \n"
						+ "sessionID : " + clientHello.sessionID + " \n"
						+ "cipherSuitesLength : " + std::to_string(clientHello.cipherSuitesLength) + " \n"
						+ "compressionMethodLength : " + std::to_string(clientHello.compressionMethodLength) + " \n"
						+ "extentionsLength : " + std::to_string(clientHello.extentionsLength) + " \n"
						+ "serverNameLength : " + std::to_string(clientHello.serverNameLength) + " \n"
						+ "serverName; : " + clientHello.serverName + " \n"
						+"#######################################################################\n";
	}
};

#endif // TLS_HPP
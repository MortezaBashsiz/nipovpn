#ifndef TLS_HPP
#define TLS_HPP

#include "config.hpp"
#include "log.hpp"

#define SERVER_NAME_LEN 256
#define TLS_HEADER_LEN 5
#define TLS_HANDSHAKE_CONTENT_TYPE 0x16
#define TLS_HANDSHAKE_TYPE_CLIENT_HELLO 0x01

#ifndef MIN
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif

class TlsRequest{
public:

	TlsRequest(Config config);
	~TlsRequest();
	Config nipoConfig;
	Log nipoLog;
	
	std::string type,
	 						version,
	 						messageType,
	 						clientVersion,
	 						clientRandomValue,
	 						sessionID,
	 						serverName;

	unsigned short int 	contentLength,
											messageLength,
											sessionIDLength,
											cipherSuitesLength,
											compressionMethodLength,
											extentionsLength,
											serverNameLength;

	unsigned short port;
	std::string data;

	void parseTlsHeader();

	std::string toString()
	{
		return 	std::string("\n#######################################################################\n")
						+ "type : " + type + " \n"
						+ "version : " + version + " \n"
						+ "messageType : " + messageType + " \n"
						+ "clientVersion : " + clientVersion + " \n"
						+ "clientRandomValue : " + clientRandomValue + " \n"
						+ "sessionID : " + sessionID + " \n"
						+ "serverName; : " + serverName + " \n"
						+ "contentLength : " + std::to_string(contentLength) + " \n"
						+ "messageLength : " + std::to_string(messageLength) + " \n"
						+ "sessionIDLength : " + std::to_string(sessionIDLength) + " \n"
						+ "cipherSuitesLength : " + std::to_string(cipherSuitesLength) + " \n"
						+ "compressionMethodLength : " + std::to_string(compressionMethodLength) + " \n"
						+ "extentionsLength : " + std::to_string(extentionsLength) + " \n"
						+ "serverNameLength : " + std::to_string(serverNameLength) + " \n"
						+"#######################################################################\n";
	}
};

#endif // TLS_HPP
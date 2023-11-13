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
	Config nipoConfig;
	Log nipoLog;
	
	std::string type = "NULL",
	 						version = "NULL",
	 						contentLength = "NULL",
	 						messageType = "NULL",
	 						messageLength = "NULL",
	 						clientVersion = "NULL",
	 						clientRandomValue = "NULL";
	unsigned short port;
	std::array<unsigned char, 8192> data;

	void parseTlsHeader();
	std::string getStrFromDataPos(unsigned short start, unsigned short end);

	TlsRequest(Config config);
	~TlsRequest();
};

#endif // TLS_HPP
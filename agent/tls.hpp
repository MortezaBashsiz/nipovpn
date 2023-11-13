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
	 						message = "NULL",
	 						messageLength = "NULL",
	 						protocolVersion = "NULL",
	 						randomValue = "NULL";
	unsigned short port;
	unsigned char* data;

	void parseTlsHeader();

	TlsRequest(Config config);
	~TlsRequest();
};

#endif // TLS_HPP
#include "tls.hpp"

TlsRequest::TlsRequest(Config config): nipoLog(config){
	nipoConfig = config;
}

TlsRequest::~TlsRequest(){
}

void TlsRequest::parseTlsHeader(){
	std::string tmpStr;
	unsigned short pos=0;
	
	tmpStr = data.substr(pos, 2);
	if (tmpStr == "16"){
		type="TLS Handshake";
		pos += 2; // 2
	
		tmpStr = data.substr(pos, 2);
		version = std::to_string(hexToInt(tmpStr));
		pos += 2; // 4
		tmpStr = data.substr(pos, 2);
		version = version+"."+std::to_string(hexToInt(tmpStr));
		pos += 2; // 6
	
		tmpStr = data.substr(pos, 4);
		contentLength = hexToInt(tmpStr);
		pos += 4; // 10
	
		tmpStr = data.substr(pos, 2);
		if (tmpStr == "01"){
			messageType="ClientHello";
		}
		pos += 2; // 12
	
		tmpStr = data.substr(pos, 6);
		messageLength = hexToInt(tmpStr);
		pos += 6; // 18
	
		tmpStr = data.substr(pos, 2);
		clientVersion = std::to_string(hexToInt(tmpStr));
		pos += 2; // 20
		tmpStr = data.substr(pos, 2);
		clientVersion = clientVersion+"."+std::to_string(hexToInt(tmpStr));
		pos += 2; // 22
	
		tmpStr = data.substr(pos, 64);
		clientRandomValue = tmpStr;

		pos += 64; // 82
	
		tmpStr = data.substr(pos, 2);
		sessionIDLength = hexToInt(tmpStr);
		pos += 2; // 84
	
		tmpStr = data.substr(pos, sessionIDLength);
		sessionID = tmpStr;
		pos = pos + (sessionIDLength * 2); // ??
	
		tmpStr = data.substr(pos, 4);
		cipherSuitesLength = hexToInt(tmpStr);
		pos += 4; // ??
		pos = pos + (cipherSuitesLength * 2); // ??

		tmpStr = data.substr(pos, 2);
		compressionMethodLength = hexToInt(tmpStr);
		pos += 2; // ??
		pos = pos + (compressionMethodLength * 2); // ??
		
		tmpStr = data.substr(pos, 4);
		extentionsLength = hexToInt(tmpStr);
		pos += 4; // ??
		
		tmpStr = data.substr(pos, 4);
		if ( hexToInt(tmpStr) == 0 ){
			pos += 14;
			tmpStr = data.substr(pos, 4);
			serverNameLength = hexToInt(tmpStr);
			pos += 4;
			tmpStr = data.substr(pos, serverNameLength * 2);
			serverName = hexToASCII(tmpStr);
		}else{
			pos = pos + (hexToInt(tmpStr) * 2);
		}
		
	}
}
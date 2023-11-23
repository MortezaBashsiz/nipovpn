#include "tls.hpp"

Tls::Tls(Config config): nipoLog(config), nipoEncrypt(config){
	nipoConfig = config;
}

Tls::~Tls(){
}

void Tls::handle(){
	parseRecordHeader();
	parseHandshakeHeader();
	if (handshakeHeader.messageType == "ClientHello"){
		parseClientHello();
		sendClientHello();
	}
}

void Tls::parseRecordHeader(){
	std::string tmpStr;
	unsigned short pos=0;
	tmpStr = data.substr(pos, 2);
	if (tmpStr == "16"){
		recordHeader.type="TLS Handshake";
		pos += 2; // 2
	
		tmpStr = data.substr(pos, 2);
		recordHeader.version = std::to_string(hexToInt(tmpStr));
		pos += 2; // 4
		tmpStr = data.substr(pos, 2);
		recordHeader.version = recordHeader.version+"."+std::to_string(hexToInt(tmpStr));
		pos += 2; // 6
	
		tmpStr = data.substr(pos, 4);
		recordHeader.contentLength = hexToInt(tmpStr);
		pos += 4; // 10
	}
}

void Tls::parseHandshakeHeader(){
	std::string tmpStr;
	unsigned short pos=10;
	tmpStr = data.substr(pos, 2);
	if (tmpStr == "01"){
		handshakeHeader.messageType="ClientHello";
	}
	pos += 2; // 12
	tmpStr = data.substr(pos, 6);
	handshakeHeader.messageLength = hexToInt(tmpStr);
	pos += 6; // 18
}

void Tls::parseClientHello(){
	std::string tmpStr;
	unsigned short pos=18;
	tmpStr = data.substr(pos, 2);
	clientHello.clientVersion = std::to_string(hexToInt(tmpStr));
	pos += 2; // 20
	tmpStr = data.substr(pos, 2);
	clientHello.clientVersion = clientHello.clientVersion+"."+std::to_string(hexToInt(tmpStr));
	pos += 2; // 22
	
	tmpStr = data.substr(pos, 64);
	clientHello.clientRandomValue = tmpStr;

	pos += 64; // 82
	
	tmpStr = data.substr(pos, 2);
	clientHello.sessionIDLength = hexToInt(tmpStr);
	pos += 2; // 84
	
	tmpStr = data.substr(pos, clientHello.sessionIDLength);
	clientHello.sessionID = tmpStr;
	pos = pos + (clientHello.sessionIDLength * 2); // ??
	
	tmpStr = data.substr(pos, 4);
	clientHello.cipherSuitesLength = hexToInt(tmpStr);
	pos += 4; // ??
	pos = pos + (clientHello.cipherSuitesLength * 2); // ??

	tmpStr = data.substr(pos, 2);
	clientHello.compressionMethodLength = hexToInt(tmpStr);
	pos += 2; // ??
	pos = pos + (clientHello.compressionMethodLength * 2); // ??
		
	tmpStr = data.substr(pos, 4);
	clientHello.extentionsLength = hexToInt(tmpStr);
	pos += 4; // ??
		
	tmpStr = data.substr(pos, 4);
	if ( hexToInt(tmpStr) == 0 ){
		pos += 14;
		tmpStr = data.substr(pos, 4);
		clientHello.serverNameLength = hexToInt(tmpStr);
		pos += 4;
		tmpStr = data.substr(pos, clientHello.serverNameLength * 2);
		clientHello.serverName = hexToASCII(tmpStr);
	}
}

std::string Tls::sendClientHello(){
	Proxy proxy(nipoConfig);
	proxy.isClientHello = true;
	unsigned char *encryptedData;
	std::string encodedData, decodedData;
	int dataLen = data.length();
	if (nipoConfig.config.encryption == "yes")
	{
		encryptedData = nipoEncrypt.encryptAes((unsigned char *)data.c_str(), &dataLen);
		nipoLog.write("Encrypted request", nipoLog.levelDebug);
		nipoLog.write((char *)encryptedData, nipoLog.levelDebug);
		nipoLog.write("Encoding encrypted request", nipoLog.levelDebug);
		encodedData = nipoEncrypt.encode64((char *)encryptedData);
	} else if(nipoConfig.config.encryption == "no") {
		encodedData = nipoEncrypt.encode64(data);
	}
	nipoLog.write("Encoded encrypted request", nipoLog.levelDebug);
	nipoLog.write(encodedData, nipoLog.levelDebug);
	nipoLog.write("Sending ClientHello request to nipoServer", nipoLog.levelDebug);
	nipoLog.write(toString(), nipoLog.levelDebug);

	proxy.request = encodedData;
	proxy.dataLen = dataLen;
	proxy.send();
	
	return proxy.response;
}
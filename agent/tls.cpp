#include "tls.hpp"

Tls::Tls(Config config): nipoLog(config), nipoEncrypt(config){
	nipoConfig = config;
}

Tls::~Tls(){
}

void Tls::detectRequestType(){
	std::string tmpStr;
	unsigned short pos=0;
	tmpStr = requestStr.substr(pos, 2);
	if (tmpStr == "16"){
		recordHeader.type="TLSHandshake";
	}
	if (tmpStr == "14"){
		recordHeader.type="ChangeCipherSpec";
	}
}

void Tls::handle(response& resp){
	std::cout << "DEBUGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG : " << std::endl << requestStr << std::endl;
	if (recordHeader.type == "TLSHandshake"){
		parseRecordHeader();
		parseHandshakeHeader();
		if (handshakeHeader.messageType == "ClientHello"){
			parseClientHello();
			send(resp);
		}
	}
	if (recordHeader.type == "ChangeCipherSpec"){
		send(resp);
	}
}

void Tls::parseRecordHeader(){
	std::string tmpStr;
	unsigned short pos=0;
	tmpStr = requestStr.substr(pos, 2);
	if (recordHeader.type == "TLSHandshake"){
		pos += 2; // 2
	
		tmpStr = requestStr.substr(pos, 2);
		recordHeader.version = std::to_string(hexToInt(tmpStr));
		pos += 2; // 4
		tmpStr = requestStr.substr(pos, 2);
		recordHeader.version = recordHeader.version+"."+std::to_string(hexToInt(tmpStr));
		pos += 2; // 6
	
		tmpStr = requestStr.substr(pos, 4);
		recordHeader.contentLength = hexToInt(tmpStr);
		pos += 4; // 10
	}
}

void Tls::parseHandshakeHeader(){
	std::string tmpStr;
	unsigned short pos=10;
	tmpStr = requestStr.substr(pos, 2);
	if (tmpStr == "01"){
		handshakeHeader.messageType="ClientHello";
	}
	pos += 2; // 12
	tmpStr = requestStr.substr(pos, 6);
	handshakeHeader.messageLength = hexToInt(tmpStr);
	pos += 6; // 18
}

void Tls::parseClientHello(){
	std::string tmpStr;
	unsigned short pos=18;
	tmpStr = requestStr.substr(pos, 2);
	clientHello.clientVersion = std::to_string(hexToInt(tmpStr));
	pos += 2; // 20
	tmpStr = requestStr.substr(pos, 2);
	clientHello.clientVersion = clientHello.clientVersion+"."+std::to_string(hexToInt(tmpStr));
	pos += 2; // 22
	
	tmpStr = requestStr.substr(pos, 64);
	clientHello.clientRandomValue = tmpStr;

	pos += 64; // 82
	
	tmpStr = requestStr.substr(pos, 2);
	clientHello.sessionIDLength = hexToInt(tmpStr);
	pos += 2; // 84
	
	tmpStr = requestStr.substr(pos, clientHello.sessionIDLength);
	clientHello.sessionID = tmpStr;
	pos = pos + (clientHello.sessionIDLength * 2); // ??
	
	tmpStr = requestStr.substr(pos, 4);
	clientHello.cipherSuitesLength = hexToInt(tmpStr);
	pos += 4; // ??
	pos = pos + (clientHello.cipherSuitesLength * 2); // ??

	tmpStr = requestStr.substr(pos, 2);
	clientHello.compressionMethodLength = hexToInt(tmpStr);
	pos += 2; // ??
	pos = pos + (clientHello.compressionMethodLength * 2); // ??
		
	tmpStr = requestStr.substr(pos, 4);
	clientHello.extentionsLength = hexToInt(tmpStr);
	pos += 4; // ??
		
	tmpStr = requestStr.substr(pos, 4);
	if ( hexToInt(tmpStr) == 0 ){
		pos += 14;
		tmpStr = requestStr.substr(pos, 4);
		clientHello.serverNameLength = hexToInt(tmpStr);
		pos += 4;
		tmpStr = requestStr.substr(pos, clientHello.serverNameLength * 2);
		clientHello.serverName = hexToASCII(tmpStr);
	}
}

void Tls::send(response& res){
	Proxy proxy(nipoConfig);
	response newResponse;
	unsigned char *encryptedData;
	std::string encodedData, decodedData;
	int dataLen = requestStr.length();
	std::string messageTypeStr="";

	if (handshakeHeader.messageType == "ClientHello"){
		proxy.isClientHello = true;
		proxy.isChangeCipherSpec = false;
		messageTypeStr = handshakeHeader.messageType;
	}
	if (recordHeader.type == "ChangeCipherSpec"){
		proxy.isChangeCipherSpec = true;
		proxy.isClientHello = false;
		messageTypeStr = recordHeader.type;
	}
	if (nipoConfig.config.encryption == "yes")
	{
		encryptedData = nipoEncrypt.encryptAes((unsigned char *)requestStr.c_str(), &dataLen);
		nipoLog.write("Encrypted " + messageTypeStr, nipoLog.levelDebug);
		nipoLog.write((char *)encryptedData, nipoLog.levelDebug);
		nipoLog.write("Encoding encrypted " + messageTypeStr, nipoLog.levelDebug);
		encodedData = nipoEncrypt.encode64((char *)encryptedData);
	} else if(nipoConfig.config.encryption == "no") {
		encodedData = nipoEncrypt.encode64(requestStr);
	}
	nipoLog.write("Encoded encrypted " + messageTypeStr, nipoLog.levelDebug);
	nipoLog.write(encodedData, nipoLog.levelDebug);
	nipoLog.write("Sending " + messageTypeStr + " to nipoServer", nipoLog.levelDebug);
	nipoLog.write(toString(), nipoLog.levelDebug);

	proxy.request = encodedData;
	proxy.dataLen = dataLen;
	proxy.send();

	nipoLog.write( messageTypeStr + "Response recieved from niposerver", nipoLog.levelDebug);
	nipoLog.write("\n"+proxy.response+"\n", nipoLog.levelDebug);
	newResponse.parse(proxy.response);
	nipoLog.write("Parsed " + messageTypeStr + " response from niposerver", nipoLog.levelDebug);
	nipoLog.write(newResponse.toString(), nipoLog.levelDebug);
	int responseContentLength;
	if ( newResponse.contentLength != "" ){
		responseContentLength = std::stoi(newResponse.contentLength);
		decodedData = nipoEncrypt.decode64(newResponse.parsedResponse.body().data());
	} else{
		responseContentLength = 0;
		decodedData = "Empty";
	}
	nipoLog.write("Decoded recieved " + messageTypeStr + " response", nipoLog.levelDebug);
	nipoLog.write(decodedData, nipoLog.levelDebug);
	if (nipoConfig.config.encryption == "yes")
	{
		char *plainData = (char *)nipoEncrypt.decryptAes((unsigned char *)decodedData.c_str(), &responseContentLength);
		nipoLog.write("Decrypt recieved " + messageTypeStr + " response from niposerver", nipoLog.levelDebug);
		nipoLog.write(plainData, nipoLog.levelDebug);
		res.content = plainData;
	} else if(nipoConfig.config.encryption == "no") {
		res.content = decodedData;
	}
	nipoLog.write("Sending " + messageTypeStr + " response to client", nipoLog.levelDebug);
	nipoLog.write(res.toString(), nipoLog.levelDebug);
}
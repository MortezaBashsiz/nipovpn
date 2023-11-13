#include "tls.hpp"

TlsRequest::TlsRequest(Config config): nipoLog(config){
	nipoConfig = config;
}

TlsRequest::~TlsRequest(){
}

void TlsRequest::parseTlsHeader(){
	std::cout << "SIZE IN TLS.CPP : " << sizeof(data) << std::endl;
	if (sizeof(data) > 0){
		std::cout << "PACKET: " << std::endl;
		if (data[0] == 0x16){
			type="TLS Handshake";
			std::cout << "type: " << type << std::endl;
		}
		version=std::to_string(data[1])+"."+std::to_string(data[2]);
		std::cout << "version: " << version << std::endl;
		
		contentLength = std::to_string(hexToInt(getStrFromDataPos(3,4)));
		std::cout << "contentLength: " << contentLength << std::endl;

		if (data[5] == 0x01){
			messageType="client hello";
			std::cout << "messageType: " << messageType << std::endl;
		}
		messageLength = std::to_string(hexToInt(getStrFromDataPos(6,8)));
		std::cout << "messageLength: " << messageLength << std::endl;

		clientVersion=std::to_string(data[9])+"."+std::to_string(data[10]);
		std::cout << "clientVersion: " << clientVersion << std::endl;
		
		clientRandomValue = getStrFromDataPos(11,42);
		std::cout << "clientRandomValue: " << clientRandomValue << std::endl;
		
		// for (int i = 0; i < sizeof(data); i+=1) {
		// 	printf("%02x ", data[i]);
		// 	if ((i + 1) % 16 == 0) {
		// 		std::cout << std::endl;
		// 	}
		// }
		// std::cout << std::endl;
	}
}

std::string TlsRequest::getStrFromDataPos(unsigned short start, unsigned short end){
	std::string result;
	for (int i=start; i<=end; i++){
		result+=std::to_string(data[i]);
	}
	return result;
}
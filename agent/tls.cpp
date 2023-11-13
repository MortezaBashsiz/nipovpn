#include "tls.hpp"

TlsRequest::TlsRequest(Config config): nipoLog(config){
	nipoConfig = config;
}

TlsRequest::~TlsRequest(){
}

void TlsRequest::parseTlsHeader(){
	std::cout << "SIZE IN TLS.CPP : " << sizeof(data) << std::endl;
	if (sizeof(data) > 0){
		// std::cout << "PACKET: " << std::endl;
		// if (data[0] == 0x16){
		// 	type="TLS Handshake";
		// 	std::cout << "type: " << type << std::endl;
		// }
		// version=std::to_string(data[1])+"."+std::to_string(data[2]);
		// std::cout << "version: " << version << std::endl;
		
		// contentLength = std::to_string(hexToInt(std::to_string(data[3])+std::to_string(data[4])));
		// std::cout << "contentLength: " << contentLength << std::endl;

		// if (data[5] == 0x01){
		// 	message="client hello";
		// 	std::cout << "message: " << message << std::endl;
		// }
		// messageLength = std::to_string(hexToInt(std::to_string(data[6])+std::to_string(data[7])+std::to_string(data[8])));
		// std::cout << "messageLength: " << messageLength << std::endl;

		
		for (int i = 0; i < sizeof(data); i+=1) {
			printf("%02x ", data[i]);
			if ((i + 1) % 16 == 0) {
				std::cout << std::endl;
			}
		}
		std::cout << std::endl;
	}
}








			// for (int i = 0; i < sizeof(data); ++i) {
			// 	// printf("%02x ", data[i]);
			// 	if ((data[i] == 0x16) && (type == "NULL"))
			// 	{
			// 		type="TLS Handshake";
			// 		std::cout << "KIIIIIIIIIIIIIIIIIIIIIIIR";
			// 	}
			// 	if ((i + 1) % 16 == 0) {
			// 		std::cout << std::endl;
			// 	}
			// }
			// std::cout << std::endl;
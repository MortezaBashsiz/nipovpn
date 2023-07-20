#include "proxy.hpp"
#include "aes.hpp"

#include <cstring>
#include <string>

std::string encrypt(std::string content, int plainLen, std::string token){
	char* plain = new char[content.length()];
	char* key = new char[token.length()];
	strcpy(plain, content.c_str());
	strcpy(key, token.c_str());
	std::cout << std::endl << std::endl << std::endl << "ENC plain : " << std::endl << std::endl << std::endl << plain << std::endl << std::endl << std::endl ;
	std::cout << std::endl << std::endl << std::endl << "ENC key : " << std::endl << std::endl << std::endl << key << std::endl << std::endl << std::endl ;
	AES aes(AESKeyLength::AES_256);
	unsigned char *out = aes.EncryptECB((unsigned char *)plain, plainLen, (unsigned char *)key);
	std::cout << std::endl << std::endl << std::endl << "ENC result : " << std::endl << std::endl << std::endl << out << std::endl << std::endl << std::endl ;
	return (char*)out;
}

std::string decrypt(std::string content, int plainLen, std::string token){
	char* plain = new char[content.length()];
	char* key = new char[token.length()];
	strcpy(plain, content.c_str());
	strcpy(key, token.c_str());
	std::cout << std::endl << std::endl << std::endl << "DEC plain : " << std::endl << std::endl << std::endl << plain << std::endl << std::endl << std::endl ;
	std::cout << std::endl << std::endl << std::endl << "DEC key : " << std::endl << std::endl << std::endl << key << std::endl << std::endl << std::endl ;
	AES aes(AESKeyLength::AES_256);
	unsigned char *out = aes.DecryptECB((unsigned char *)plain, plainLen, (unsigned char *)key);
	std::cout << std::endl << std::endl << std::endl << "DEC result : " << std::endl << std::endl << std::endl << out << std::endl << std::endl << std::endl ;
	return (char*)out;
}
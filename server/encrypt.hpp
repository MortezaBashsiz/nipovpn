#ifndef ENCRYPT_HPP
#define ENCRYPT_HPP

#include <openssl/evp.h>
#include <openssl/aes.h>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string.hpp>

#include "config.hpp"
#include "log.hpp"

class Encrypt
{
public:
	Config nipoConfig;
	Log nipoLog;
	EVP_CIPHER_CTX* encryptEvp = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX* decryptEvp = EVP_CIPHER_CTX_new();
	unsigned char *encryptAes(unsigned char *plaintext, int *len);
	unsigned char *decryptAes(unsigned char *ciphertext, int *len);
	std::string decode64(const std::string &val);
	std::string encode64(const std::string &val);	
	
	Encrypt(Config config);
	~Encrypt();
	
private:
	int salt[2] = {00000, 00000};
	unsigned char *token;
	int tokenLen;
	int initAes(unsigned char *key_data, int key_data_len, unsigned char *salt, EVP_CIPHER_CTX *e_ctx, EVP_CIPHER_CTX *d_ctx);
};

#endif //ENCRYPT_HPP
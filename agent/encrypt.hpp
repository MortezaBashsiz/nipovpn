#ifndef ENCRYPT_HPP
#define ENCRYPT_HPP

#include <openssl/evp.h>
#include <openssl/aes.h>

#include "config.hpp"
#include "log.hpp"

class Encrypt
{
public:
	EVP_CIPHER_CTX* encryptEvp = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX* decryptEvp = EVP_CIPHER_CTX_new();
	int salt[2] = {00000, 00000};
	unsigned char *token;
	int tokenLen;
	Config nipoConfig;
	Log nipoLog;

	Encrypt(Config config);
	~Encrypt();
	int initAes(unsigned char *key_data, int key_data_len, unsigned char *salt, EVP_CIPHER_CTX *e_ctx, EVP_CIPHER_CTX *d_ctx);
	unsigned char *encryptAes(EVP_CIPHER_CTX *e, unsigned char *plaintext, int *len);
	unsigned char *decryptAes(EVP_CIPHER_CTX *e, unsigned char *ciphertext, int *len);
};



#endif //ENCRYPT_HPP
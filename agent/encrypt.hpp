#ifndef ENCRYPT_HPP
#define ENCRYPT_HPP

#include <openssl/evp.h>
#include <openssl/aes.h>

int initAes(unsigned char *key_data, int key_data_len, unsigned char *salt, EVP_CIPHER_CTX *e_ctx, 
             EVP_CIPHER_CTX *d_ctx);
unsigned char *encryptAes(EVP_CIPHER_CTX *e, unsigned char *plaintext, int *len);
unsigned char *decryptAes(EVP_CIPHER_CTX *e, unsigned char *ciphertext, int *len);

#endif //ENCRYPT_HPP
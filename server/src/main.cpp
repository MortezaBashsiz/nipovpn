#include "config.hpp"
#include "log.hpp"
#include "encrypt.hpp"

int main(int argc=0, char* argv[]=0){
	mainArgs mainArgs = validateMainArgs(argc, argv);
	Config nipoConfig(mainArgs.configPath);
	Log nipoLog(nipoConfig.config.logFile, nipoConfig.config.logLevel);
	
	unsigned char plain[] = "This is text";
	unsigned char key[] = "28e1a861-a2ce-4800-8c49-01431c9acc32";
	unsigned int plainLen = 16 * sizeof(plain);

	AES aes(AESKeyLength::AES_256);  ////128 - key length, can be 128, 192 or 256
	// auto c = aes.EncryptECB(plain, plainLen, key);
	unsigned char *out = aes.EncryptECB(plain, plainLen, key);
  unsigned char *innew = aes.DecryptECB(out, plainLen, key);
	std::cout << "FUCK: " << out << endl;
	std::cout << "FUCK: " << innew << endl;
	
	bool result = nipoLog.write("bbbb", 1, nipoLog.type.warn);
};
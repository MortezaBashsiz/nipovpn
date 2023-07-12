#include "config.hpp"
#include "log.hpp"
#include "encrypt.hpp"

int main(int argc=0, char* argv[]=0){
	mainArgs mainArgs = validateMainArgs(argc, argv);
	Config nipoConfig(mainArgs.configPath);
	Log nipoLog(nipoConfig.config.logFile, nipoConfig.config.logLevel);
	
	unsigned char plain[] = "This is text";
	unsigned char key[] = "iuwahchu5aingohtoutoiveing9ahd4GoiZo4dai9eezaeloov6Kae4taiquooGohk3ERaR8Cheid8tae4Quohgiash7fai8giuShaingee4be0oib6ahbaich1veo2MeeYoh2Ophuoxafie1ge1wei3moo7shainied3eez5oe2uculohs8Cu8OoquaePee1weirie4eCunohb4Hei4eo5Oopukon6quath5mieGee4eivooCohmai5uu7Aex7v";
	unsigned int plainLen = 16 * sizeof(plain);

	AES aes(AESKeyLength::AES_256);
	unsigned char *out = aes.EncryptECB(plain, plainLen, key);
  unsigned char *innew = aes.DecryptECB(out, plainLen, key);
	std::cout << "ENCRYPTED: " << out << endl;
	std::cout << "PLAIN: " << innew << endl;

	bool result = nipoLog.write("TEST", 1, nipoLog.type.warn);
};
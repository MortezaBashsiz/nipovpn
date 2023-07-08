#include "main.h"

int main(){
	Config nipoConfig("/home/morteza/data/git/MortezaBashsiz/nipovpn/files/config/nipoServerConfig.json");
	// Config nipoConfig;
	cout << "config port is: " << nipoConfig.config.port << "\n";
	cout << "config thread is: " << nipoConfig.config.threads << "\n";
	cout << "config sslKey is: " << nipoConfig.config.sslKey << "\n";
	cout << "config user 0 token: " << nipoConfig.config.users[0].token << "\n";
};
#include "main.h"

int main()
{
    Config config("/home/morteza/data/git/MortezaBashsiz/nipovpn/files/config/nipoServerConfig.json");
    // Config config;
    cout << "config port is: " << config.port << "\n";
    cout << "config thread is: " << config.threads << "\n";
    cout << "config sslKey is: " << config.sslKey << "\n";
    cout << "config user 0 token: " << config.users[1].token << "\n";
}
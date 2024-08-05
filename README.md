# NipoVPN
A powerfull proxy to hide your http packets inside a fake http request.

## Usecases

### Bypass filtering
This could help you to bypass the filtering in your country.

![enter image description here](https://github.com/MortezaBashsiz/nipovpn/blob/main/files/pic/archFilternet.png)

### Hide your requests in the Internet
If you want to hide your HTTP requests in the Internet, this proxy could help you.

![enter image description here](https://github.com/MortezaBashsiz/nipovpn/blob/main/files/pic/archInternet.png)

## Flow
Here you can see the logical flow of a single request from first step to get the response
![enter image description here](https://github.com/MortezaBashsiz/nipovpn/blob/main/files/pic/flow.png)

## Installation

### Source

#### Dependencies
This program needs following dependencies
```bash
libyaml-cpp >= 0.8
boost-libs >= 1.83.0
```

#### Make
You can clone the repository and compile it yourself
```bash
[~/nipovpn]>$ make 
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/tcpserver.o src/tcpserver.cpp -Llib -lyaml-cpp -lssl -lcrypto
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/main.o src/main.cpp -Llib -lyaml-cpp -lssl -lcrypto
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/agenthandler.o src/agenthandler.cpp -Llib -lyaml-cpp -lssl -lcrypto
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/config.o src/config.cpp -Llib -lyaml-cpp -lssl -lcrypto
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/tcpclient.o src/tcpclient.cpp -Llib -lyaml-cpp -lssl -lcrypto
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/http.o src/http.cpp -Llib -lyaml-cpp -lssl -lcrypto
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/tcpconnection.o src/tcpconnection.cpp -Llib -lyaml-cpp -lssl -lcrypto
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/log.o src/log.cpp -Llib -lyaml-cpp -lssl -lcrypto
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/serverhandler.o src/serverhandler.cpp -Llib -lyaml-cpp -lssl -lcrypto
mkdir -p build/
g++ -Wall -Wextra -std=c++20 -ggdb  -MMD -MP -Iinclude -Isrc -c -o build/runner.o src/runner.cpp -Llib -lyaml-cpp -lssl -lcrypto
Building...
mkdir -p nipovpn/usr/bin/
g++ build/tcpserver.o build/main.o build/agenthandler.o build/config.o build/tcpclient.o build/http.o build/tcpconnection.o build/log.o build/serverhandler.o build/runner.o -o nipovpn/usr/bin/nipovpn -Llib -lyaml-cpp -lssl -lcrypto
[~/nipovpn]>$

```

#### Create directories
Create the log directory and log file
```bash
[~/nipovpn]>$ sudo mkdir /var/log/nipovpn/
[~/nipovpn]>$ sudo touch /var/log/nipovpn/nipovpn.log 
```

#### Run
Run it
```bash
[~/nipovpn]>$ sudo nipovpn/usr/bin/nipovpn server nipovpn/etc/nipovpn/config.yaml
2024-08-02_15:30:07 [INFO] Config initialized in server mode 
2024-08-02_15:30:07 [INFO] 
Config :
 General :
   fakeUrl: http://www.adas.com/api01
   method: HEAD
   timeWait: 0
   repeatWait: 1
 Log :
   logLevel: DEBUG
   logFile: /var/log/nipovpn/nipovpn.log
 server :
   threads: 4
   listenIp: 0.0.0.0
   listenPort: 443
 agent :
   threads: 4
   listenIp: 0.0.0.0
   listenPort: 8080
   serverIp: 127.0.0.1
   serverPort: 443
   token: af445adb-2434-4975-9445-2c1b2231
   httpVersion: 1.1
   userAgent: NipoAgent 
```

### Package
Currently it is only available for debian based Linuxs and fully tested on ubuntu 24.04
You can simply install the package
```bash
[~/nipovpn]>$ wget https://github.com/MortezaBashsiz/nipovpn/raw/main/files/deb/nipovpn.deb
[~/nipovpn]>$ apt install ./nipovpn.deb
```

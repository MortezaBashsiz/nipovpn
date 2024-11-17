# NipoVPN (Not ready yet :D)

## Overview

NipoVPN is a powerful proxy tool designed to conceal your HTTP requests within fake HTTP requests. This program, written in C++, leverages the Boost library to handle networking functionalities efficiently.


## Features

  - HTTP Request Obfuscation: Hide your legitimate HTTP requests inside decoy requests to avoid detection.
  - Boost Library Integration: Utilizes Boost for robust and reliable networking operations.
  - High Performance: Optimized for speed and efficiency, ensuring minimal impact on request latency.


## Usecases

### Bypass filtering
This could help you to bypass the filtering in your country.

![Filtering](https://github.com/MortezaBashsiz/nipovpn/blob/main/files/pic/archFilternet.png)

### Hide your requests in the Internet
If you want to hide your HTTP requests in the Internet, this proxy could help you.

![Internet](https://github.com/MortezaBashsiz/nipovpn/blob/main/files/pic/archInternet.png)

## Flow
Here you can see the logical flow of a single request from first step to get the response
![Flow](https://github.com/MortezaBashsiz/nipovpn/blob/main/files/pic/flow.png)


## Contribution
To contribute take a look at [contribution page](https://github.com/MortezaBashsiz/nipovpn/blob/main/CONTRIBUTING.md).

## Build

Build from source for [Linux](guides/BuildLinux.md)



## Run

#### Create directories
Create the log directory and log file
```bash
[~/nipovpn]>$ sudo mkdir /var/log/nipovpn/
[~/nipovpn]>$ sudo touch /var/log/nipovpn/nipovpn.log 
```

Run it
```bash
sudo build/core/nipovpn server nipovpn/etc/nipovpn/config.yaml
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

#### Install
Currently it is only available for Debian-based Linuxes and fully tested on ubuntu 24.04
You can simply download and install the package. To download it please visit the [releases page](https://github.com/MortezaBashsiz/nipovpn/tags) and get the latest release.
```bash
[~]>$ sudo apt install ./nipovpn.deb
```

#### Run
There are systemd services to manage the nipovpn process.
`nipovpn-server.service` to manage as server.
`nipovpn-agent.service` to manage as agent.
```bash
[~]>$ sudo systemctl start nipovpn-server.service
[~]>$ cat /var/log/nipovpn/nipovpn.log
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

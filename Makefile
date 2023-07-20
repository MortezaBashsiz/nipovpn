#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
INCLUDEPATH=../files/include
SRC=./src
server:
	cd $(SRC) &&	$(CC) -I$(INCLUDEPATH) -Iserver -Icommon $(CFLAGS) -o ../niposerver niposerver.cpp common/general.cpp server/server.cpp server/response.cpp server/request.cpp server/connection.cpp common/config.cpp common/log.cpp common/aes.cpp server/proxy.cpp
agent:
	cd $(SRC) &&	$(CC) -I$(INCLUDEPATH) -Iagent -Icommon $(CFLAGS) -o ../nipoagent nipoagent.cpp common/general.cpp agent/agent.cpp agent/response.cpp agent/request.cpp agent/connection.cpp common/config.cpp common/log.cpp common/aes.cpp agent/proxy.cpp
clean:
	rm niposerver
	rm nipoagent

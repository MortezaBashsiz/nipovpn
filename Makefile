#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
INCLUDEPATH=../files/include
SRC=./src
all:
	cd $(SRC) &&	$(CC) -I$(INCLUDEPATH) $(CFLAGS) -o ../nipovpn main.cpp general.cpp server.cpp response.cpp serverRequest.cpp connection.cpp config.cpp log.cpp aes.cpp
clean:
	rm nipovpn

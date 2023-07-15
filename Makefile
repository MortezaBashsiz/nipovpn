#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
INCLUDEPATH=../files/include
SRC=./src
all:
	cd $(SRC) &&	$(CC) -I$(INCLUDEPATH) $(CFLAGS) -o ../nipovpn main.cpp general.cpp net.cpp response.cpp request.cpp config.cpp log.cpp encrypt.cpp
clean:
	rm nipovpn

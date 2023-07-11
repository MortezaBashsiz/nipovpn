#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
INCLUDEPATH=../../files/include
all: main.cpp general.cpp net/net.cpp config/config.cpp log/log.cpp
	$(CC) -I$(INCLUDEPATH) $(CFLAGS) -o niposerver main.cpp general.cpp net/net.cpp config/config.cpp log/log.cpp
run:
	./niposerver
clean:
	rm niposerver

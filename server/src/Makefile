#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=gcc
CFLAGS=-lstdc++ -g -ljsoncpp
all: main.cpp general.cpp net/net.cpp config/config.cpp log/log.cpp
	$(CC) $(CFLAGS) -o niposerver main.cpp general.cpp net/net.cpp config/config.cpp log/log.cpp
run:
	./niposerver
allrun: main.cpp general.cpp general.cpp net/net.cpp config/config.cpp log/log.cpp
	$(CC) $(CFLAGS) -o niposerver main.cpp general.cpp net/net.cpp config/config.cpp log/log.cpp
	./niposerver
clean:
	rm niposerver

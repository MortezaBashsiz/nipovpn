#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
all: main.cpp net/net.cpp config/config.cpp log/log.cpp
	$(CC) $(CFLAGS) -o niposerver main.cpp net/net.cpp config/config.cpp log/log.cpp
run:
	./niposerver
clean:
	rm niposerver

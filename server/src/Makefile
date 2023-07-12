#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
INCLUDEPATH=../../files/include
all: main.cpp general.cpp net.cpp config.cpp log.cpp encrypt.cpp
	$(CC) -I$(INCLUDEPATH) $(CFLAGS) -o niposerver main.cpp general.cpp net.cpp config.cpp log.cpp encrypt.cpp
run:
	./niposerver
clean:
	rm niposerver

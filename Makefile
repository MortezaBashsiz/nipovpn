#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
SRC=./server

all: server

server: config.o general.o log.o
	cd $(SRC) && $(CC) $(CFLAGS) -o ../niposerver main.cpp config.o log.o general.o

config.o:
	cd $(SRC) && $(CC) $(CFLAGS) -c config.cpp

log.o:
	cd $(SRC) && $(CC) $(CFLAGS) -c log.cpp

general.o:
	cd $(SRC) && $(CC) $(CFLAGS) -c general.cpp

clean:
	rm $(SRC)/*.o
	rm niposerver


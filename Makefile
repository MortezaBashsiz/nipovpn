#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
SRC=./server
INCLUDEPATH=../files/include

all: server

server: config general log net
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -o ../niposerver main.cpp config.o log.o general.o net.o

config:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c config.cpp

log:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c log.cpp

general:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c general.cpp

net:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c net.cpp

clean:
	rm $(SRC)/*.o
	rm niposerver


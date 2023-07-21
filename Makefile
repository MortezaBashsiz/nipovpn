#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
SRC=./server
INCLUDEPATH=../files/include

all: config log general connection request response
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -o ../niposerver main.cpp server.cpp config.o log.o general.o connection.o request.o response.o

config:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c config.cpp

log:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c log.cpp

general:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c general.cpp

connection:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c connection.cpp 

request:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c request.cpp 

response:
	cd $(SRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c response.cpp 

clean:
	rm $(SRC)/*.o
	rm niposerver


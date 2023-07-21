#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp
SERVERSRC=./server
AGENTSRC=./agent
INCLUDEPATH=../files/include

server: serverConfig serverLog serverGeneral serverConnection serverRequest serverResponse
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -o ../niposerver main.cpp server.cpp config.o log.o general.o connection.o request.o response.o

serverConfig:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c config.cpp

serverLog:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c log.cpp

serverGeneral:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c general.cpp

serverConnection:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c connection.cpp 

serverRequest:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c request.cpp 

serverResponse:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c response.cpp 

cleanserver:
	rm $(SERVERSRC)/*.o
	rm niposerver

agent: agentConfig agentLog agentGeneral
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -o ../nipoagent main.cpp config.o log.o general.o

agentConfig:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c config.cpp

agentLog:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c log.cpp

agentGeneral:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -I$(INCLUDEPATH) -c general.cpp

cleanagent:
	rm $(AGENTSRC)/*.o
	rm nipoagent

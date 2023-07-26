#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -ljsoncpp -lcrypto -lssl
SERVERSRC=./server
AGENTSRC=./agent

all: server agent 
clean: cleanserver cleanagent
	
server: serverConfig serverLog serverGeneral serverConnection serverRequest serverResponse
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -o ../niposerver main.cpp server.cpp config.o log.o general.o connection.o request.o response.o

serverConfig:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c config.cpp

serverLog:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c log.cpp

serverGeneral:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c general.cpp

serverConnection:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c connection.cpp 

serverRequest:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c request.cpp 

serverResponse:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c response.cpp 

cleanserver:
	rm $(SERVERSRC)/*.o
	rm niposerver

agent: agentConfig agentLog agentGeneral agentConnection agentRequest agentResponse agentEncrypt
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -o ../nipoagent main.cpp agent.cpp config.o log.o general.o connection.o request.o response.o encrypt.o

agentConfig:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c config.cpp

agentLog:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c log.cpp

agentGeneral:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c general.cpp

agentConnection:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c connection.cpp 

agentRequest:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c request.cpp 

agentResponse:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c response.cpp 

agentEncrypt:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c encrypt.cpp 

cleanagent:
	rm $(AGENTSRC)/*.o
	rm nipoagent

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
	
server: serverConfig serverLog serverGeneral serverSession serverRequest serverResponse serverEncrypt serverProxy
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -o ../niposerver main.cpp server.cpp config.o log.o general.o session.o request.o response.o encrypt.o proxy.o

serverConfig:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c config.cpp

serverLog:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c log.cpp

serverGeneral:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c general.cpp

serverSession:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c session.cpp 

serverRequest:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c request.cpp 

serverResponse:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c response.cpp 

serverEncrypt:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c encrypt.cpp 

serverProxy:
	cd $(SERVERSRC) && $(CC) $(CFLAGS) -c proxy.cpp 

cleanserver:
	rm $(SERVERSRC)/*.o
	rm niposerver

agent: agentConfig agentLog agentGeneral agentSession agentRequest agentResponse agentEncrypt agentProxy agentTls
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -o ../nipoagent main.cpp agent.cpp config.o log.o general.o session.o request.o response.o encrypt.o proxy.o tls.o

agentConfig:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c config.cpp

agentLog:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c log.cpp

agentGeneral:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c general.cpp

agentSession:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c session.cpp 

agentRequest:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c request.cpp 

agentResponse:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c response.cpp 

agentEncrypt:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c encrypt.cpp 

agentProxy:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c proxy.cpp 

agentTls:
	cd $(AGENTSRC) && $(CC) $(CFLAGS) -c tls.cpp 

cleanagent:
	rm $(AGENTSRC)/*.o
	rm nipoagent

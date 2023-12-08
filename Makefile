#
# Makefile for many UNIX compilers using the
# "standard" command name CC
#
CC=g++
CFLAGS=-g -lyaml-cpp
SRC=./src

all: nipovpn
cleanserver:
	rm $(SRC)/*.o
	rm niposerver

nipovpn: 
	cd $(SRC) && $(CC) $(CFLAGS) -o ../build/nipovpn main.cpp general.cpp
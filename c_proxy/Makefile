# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = g++
CFLAGS = -g -Wall -std=c++14
LDFLAGS = -lpthread

all: proxy

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.cpp
	$(CC) $(CFLAGS) -c proxy.cpp

proxy: proxy.o csapp.o 
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz


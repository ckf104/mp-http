# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = g++
CFLAGS = -g -Wall -std=c++17
LDFLAGS = -lpthread

all: proxy

helper.o: helper.cpp helper.hpp
	$(CC) $(CFLAGS) -c helper.cpp

proxy.o: proxy.cpp
	$(CC) $(CFLAGS) -c proxy.cpp

proxy: proxy.o helper.o
	$(CC) $(CFLAGS) proxy.o helper.o -o proxy $(LDFLAGS)

clean:
	rm -f *~ *.o proxy core *.tar *.zip *.gzip *.bzip *.gz


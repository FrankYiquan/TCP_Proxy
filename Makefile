CC=gcc
CFLAGS=-Wall -g

tcpproxy: tcpproxy.c
	$(CC) $(CFLAGS) -o tcpproxy tcpproxy.c
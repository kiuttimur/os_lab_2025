CC=gcc
CFLAGS=-Wall -Wextra -O2

all: tcp_client tcp_server udp_client udp_server

tcp_client: tcp_client.c
	$(CC) $(CFLAGS) -o tcp_client tcp_client.c

tcp_server: tcp_server.c
	$(CC) $(CFLAGS) -o tcp_server tcp_server.c

udp_client: udp_client.c
	$(CC) $(CFLAGS) -o udp_client udp_client.c

udp_server: udp_server.c
	$(CC) $(CFLAGS) -o udp_server udp_server.c

clean:
	rm -f tcp_client tcp_server udp_client udp_server

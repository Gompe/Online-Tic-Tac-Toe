all: server client

server: server.o
	cc -g -o server server.o -lpthread

server.o: server.c
	cc -c -Wall -g server.c

client: client.o
	cc -g -o client client.o -lpthread

client.o: client.c
	cc -c -Wall -g client.c

clean:
	rm -f  server server.o client client.o

server.o: server.c server.h
client.o: client.c client.h

TARGET=client server figlet
CC=gcc
CFLAGS=-Wall -g

normal: $(TARGET)

client: client.c
	$(CC) $(CFLAGS) client.c -o client

server: server.c
	$(CC) $(CFLAGS) server.c -o server

figlet:
	sudo apt install figlet

clean:
	$(RM) $(TARGET)


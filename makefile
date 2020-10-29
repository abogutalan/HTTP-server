CFLAGS = -Wall -g -std=c11

all: server 

server: server.c 
	gcc $(CFLAGS) -g server.c -o server


clean:
	rm -rf server server.dSYM .vscode

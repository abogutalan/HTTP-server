all: server 

server: server.c 
	gcc -g server.c -o server


clean:
	rm -rf server server.dSYM .vscode

/*
** server.c -- a stream socket server demo
** Beej’s Guide to Network Programming is Copyright © 2019 Brian “Beej Jorgensen” Hall.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <fcntl.h> //fcrttrc
#define MAXDATASIZE 4096 // max number of bytes we can get at once


#define PORT "39732"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold


void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void client(int fd, int order)
{
	int file_fd;
	long i, ret, len;
	static char buffer[MAXDATASIZE+1]; /* static so zero filled */

	ret =read(fd,buffer,MAXDATASIZE); 	/* read Web request in one go */
	
	// fprintf(stderr, "request: %s %d\n", buffer, fd);
	
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		fprintf(stderr, "ERROR failed to read browser request\n");
		exit(1);
	}
	if(ret > 0 && ret < MAXDATASIZE)	/* return code is valid chars */
		buffer[ret]=0;		/* terminate the buffer */
	else buffer[0]=0;
	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
	if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
		fprintf(stderr, "Only simple GET operation supported\n");
		exit(1);
	}
	for(i=4;i<MAXDATASIZE;i++) { /* null terminate after the second space to ignore extra stuff */
		if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			buffer[i] = 0;
			break;
		}
	}
	
	if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {  /* open the file for reading */
		fprintf(stderr, "ERROR failed to open file\n");
		exit(1);
	}

	len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
	      (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
          (void)sprintf(buffer,"HTTP/1.1 200 OK\nContent-Length: %ld\nConnection: close\n\n", len); 
	fprintf(stderr, "Header %s %d\n", buffer, order);
	
	(void)write(fd,buffer,strlen(buffer));

	/* send file in blocks */
	while (	(ret = read(file_fd, buffer, MAXDATASIZE)) > 0 ) {
		(void)write(fd,buffer,ret);
	}
	sleep(1);	/* allow socket to drain before signalling the socket is closed */
	close(fd);
	exit(1);
}


int main(void)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	int order = 1; // request order

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}


	printf("server running on port %s ...\n", PORT);

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			// if (send(new_fd, "Hello, world!", 13, 0) == -1)
			// 	perror("send");
			client(new_fd,order); 
			printf("Hello World! \n");
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this

		order++;
	}

	return 0;
}

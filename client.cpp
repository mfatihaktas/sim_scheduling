/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "4000" // the port client will be connecting to 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

#define MAXSENDLENGTH 1000
#define MAXRECVLENGTH 100
int nofChunksToSend=0;
int sockfd;
FILE *file; //For debugging purposes
//Some buffers necessary for communication
char send_buf[MAXSENDLENGTH];
char recv_buf[MAXSENDLENGTH];

//Method pre_decs
int sendTo(int socketFD, char* send_buf);
int recvFrom(int socketFD);
void close_();

char* server_address;
char* server_port;
char* nofChunksToRx;
char* fname;

int main(int argc, char *argv[])
{
	if (argc != 5) {
        fprintf(stderr,"usage: client hostname, # of chunks to be received, file name\n");
        exit(1);
    }
   	server_address = new char[strlen(argv[1])];
	memcpy(server_address, argv[1], strlen(argv[1]));
	server_port = new char[strlen(argv[2])];
	memcpy(server_port, argv[2], strlen(argv[2]));
	nofChunksToRx = new char[strlen(argv[3])];
	memcpy(nofChunksToRx, argv[3], strlen(argv[3]));
	fname = new char[strlen(argv[4])];
	memcpy(fname, argv[4], strlen(argv[4]));
	//////////////////////////////////////////////////
    int numbytes;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(server_address, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo); // all done with this structure
	
	//Communicate with the server
	recvFrom(sockfd); //recv the inquiry of server
	if(strcmp(recv_buf, "WDW?")==0){
		printf("WDW? received\n");
		sendTo(sockfd, nofChunksToRx); //the # of chunks that client desires to get
	}

	file = fopen(fname, "w");
	if (file == NULL){//If the file cannot be opened -> Do not proceed
		perror ("Error opening file");
		exit(1);
	}
	//Receive the bulk data from the server
	while(1){
		numbytes=recvFrom(sockfd);
		printf("numbytes recved: %d\n",numbytes);
		printf("CLIENT RECEIVED: '%s'\n",recv_buf);
		fprintf(file,"%s",recv_buf);
		memset(recv_buf, 0, MAXRECVLENGTH);
	}
	close_();
    return 0;
}

void close_()
{
	close(sockfd); //Socket will be closed by the server when all of the data is sent
	fclose(file);
	delete[] server_address;
	delete[] server_port;
	delete[] nofChunksToRx;
	delete[] fname;
}

int sendTo(int socketFD, char* send_buf){
	if (send(socketFD, send_buf, (int)strlen(send_buf), 0) != -1){
		return 1; //sent successfully
	}
	else{
        perror("send");
		return -1; //could not be sent
	}
}
//
int recvFrom(int socketFD){
	int numbytes = recv(socketFD, recv_buf, MAXRECVLENGTH, 0); //No flags are necessary
	if(numbytes>0){//Successful reception
		return numbytes;
	}	
	else if (numbytes == 0) {//socket is closed by the other side
		close_();
	    exit(1);
	}
	else{ //Unsuccessful reception
		perror("recv");
		return -1;
	}
}


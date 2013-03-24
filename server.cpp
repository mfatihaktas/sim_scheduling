/*
** server.c -- a stream socket server
*/
#include <iostream>
#include <fstream>
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

using namespace std;

#define BACKLOG 10     // how many pending connections queue will hold
void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

#define MAXSENDLENGTH 50
#define MAXRECVLENGTH 100
int nofChunksToSend=0;
//Some buffers necessary for communication
char send_buf[MAXSENDLENGTH];
char recv_buf[MAXRECVLENGTH];
char temp_buffer[10];

int sendTo(int socketFD, char* send_buf);
int recvFrom(int socketFD);
char* readNextFileSegment(ifstream* p_ifs, unsigned int segment_size);
void sendFile(ifstream* p_ifs, int socket_fd);

int main(int argc, const char *argv[])
{
	if(argc != 3){//Check if the args are correctly entered
		cout<< "Problem with the arguments {server_port, file name}. Exiting !\n";
		return 0;
	}
	char* fname = new char[strlen(argv[2])];
	memcpy(fname, argv[2], strlen(argv[2]));
	char* port = new char[strlen(argv[1])];
	memcpy(port, argv[1], strlen(argv[1]));
	////////////////////////////////////////////////////////////////
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
	pid_t childPID; //control variable for multithreading
	//
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
   	//For reading data from file 
   	ifstream ifs( fname, ifstream::in);
//For debugging
/*
	FILE *file; 
	file = fopen("file_server.txt", "w");
	fprintf(file,"%s","Top of the file");
*/

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
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

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

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

    printf("server: waiting for connections...\n");
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
		//fprintf(file,"%s","server: got connection from");
		//fprintf(file,"%s\n",s);
		//Threading
		childPID=fork();
		if(childPID>=0)
		{
			if(childPID == 0){ //Child process
				close(sockfd); // child doesn't need the listener
				//Send "WDW?"; What do you want?
				sendTo(new_fd, "WDW?");
				recvFrom(new_fd);
				nofChunksToSend=atoi(recv_buf);
				//send the bulk data to the client
				//Instead of sending HelloWorld messages tx a file
				sendFile(&ifs, new_fd);
				
			    close(new_fd);
				exit(0);
			}
			else{//parent process
				close(new_fd);  // parent doesn't need this
				//Listen the channel with inf while
			}
		}
		else{
			printf("\n Fork failed, exit !\n");
	        return 1;
		}
	}
    return 0;
}

void sendFile(ifstream* p_ifs, int socket_fd){
	cout<<"\nFile began to be txed;\n";

	char* line_buffer = readNextFileSegment(p_ifs, MAXSENDLENGTH);
	while(line_buffer != NULL){
		//cout<<"--------------------------------------\n";
		//cout<<"Line will be sent: "<<line_buffer<<"\nwith size: "<<strlen(line_buffer)<<endl;
		// Send the segment read from file
		if (sendTo(socket_fd, line_buffer) == 1){
			cout<<"File Segment is sent, size: "<<strlen(line_buffer)<<"\n";
			cout<<"line_buffer: "<<line_buffer<<"\n";
		}
		memset(line_buffer, 0, MAXSENDLENGTH);
		delete[] line_buffer;//delete the dynamically allocated space
		line_buffer = NULL;
		line_buffer = readNextFileSegment(p_ifs, MAXSENDLENGTH); //Read the next segment from the file to be sent
	}
	cout<<"File tx is finished !\n";
}

char* readNextFileSegment(ifstream* p_ifs, unsigned int segment_size){
	if(p_ifs->is_open()){
		char* buffer = new char[segment_size];
		unsigned int counter=0;
		char temp;
		while (counter < segment_size){
			temp = p_ifs->get(); // get character from file;
			if(p_ifs->good()){//not eof char
				buffer[counter] = temp;
			}
			else{//eof is reached
				p_ifs->close();
				return buffer;
			}
    	  	++counter;
		}
		return buffer;
	}
	else{
		return NULL;
	}
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
	printf("Server recved: %s", recv_buf);
	if(numbytes>0){//Successful reception
		return 1;
	}	
	else if (numbytes == 0) {//socket is closed by the other side
	    exit(1);
	}
	else{ //Unsuccessful reception
		perror("recv");
		return -1;
	}
}

/* Belki lazim olabilir ilerde
unsigned int nof_chunksent=0;
while(nof_chunksent < nofChunksToSend){
	strcpy(send_buf, "Hello World birader");
	sprintf(temp_buffer, "%d", nof_chunksent);
	strcat(send_buf, temp_buffer);
	//printf("send_buf: %s\n", send_buf);
	//send send_buf to receiver
	if (sendTo(new_fd, send_buf) == 1){
		++nof_chunksent;
	}
	memset(send_buf, 0, MAXSENDLENGTH);//Initialize for the next chunk sending session
	memset(temp_buffer, 0, 10);
}//All bulk data sent, close the socket
*/

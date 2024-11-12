#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_BACKLOG 5 
#define MAX_MESSAGE_SIZE 1000
#define PORT "5566"

//handle da zombies 
void sigchild_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

//determine if its ipv4 or ipv6?
void* get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int setup_socket() {

    //declarations for socket
    struct addrinfo hints;      //will be used as 3rd param in getaddrinfo() (give getaddrinfo 'hints' about how u want to use it?)
    struct addrinfo* servinfo;  //pass as 4th param of getaddrinfo to get the linked list of addrinfo structs

//socket
    memset(&hints, 0, sizeof(struct addrinfo));// zero out memory
    hints.ai_family = AF_UNSPEC;        // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // TCP stream
    hints.ai_flags = AI_PASSIVE;        // fill in IP for me (don't have to hardcode it)

    //getaddrinfo()
    //ig getaddrinfo should return 0 if its successful
    if((getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        printf("\e[1;31mERROR: getaddrinfo() did not return 0 as expected\e[0;37m\n");
        return -1;
    }

    //loop through addrinfo linked list until we can create and bind a socket with it
    struct addrinfo* ai_ptr;
    int sockfd; //socket ID?
    int yes = 1; //for setsockopt()... 4th param
    for(ai_ptr = servinfo; ai_ptr != NULL; ai_ptr = ai_ptr->ai_next) {
        //socket()
        if((sockfd = socket(ai_ptr->ai_family, ai_ptr->ai_socktype, ai_ptr->ai_protocol)) == -1) {
            printf("\e[1;33mWARNING: socket() returned -1\e[0;37m\n");
            continue; //go to next addrinfo struct in linked list
        }
        //setsockopt() <- allows to rerun server without having to wait a minute for the socket to clear after last run
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            printf("\e[1;31mERROR: setsockopt() returned -1\e[0;37m\n");
            return -1;
        }
        //bind() <- associate socket with an actual port
        if(bind(sockfd, ai_ptr->ai_addr, ai_ptr->ai_addrlen) == -1) {
            close(sockfd); //close the socket we created since we can't bind it 
            printf("\e[1;33mWARNING: bind() returned -1\e[0;37m\n");
            continue; //go to next addrinfo struct in linked list
        }
        
        break; //end loop once we've successfully created/bound a socket 
    }

    freeaddrinfo(servinfo); //done with addrinfo struct

    //check that socket was actually bound
    if(ai_ptr == NULL) {
        printf("\e[1;31mERROR: failed to bind\e[0;37m\n");
        return -1;
    }

    //listen() <- wait for incoming connections with 
    if(listen(sockfd, MAX_BACKLOG) == -1) {
        printf("\e[1;31mERROR: listen() returned -1\e[0;37m\n");
        return -1;
    }
//socket

    return sockfd;

}

int main() {
    //server name string
    const char* server_name = "Server of Joshua Davis"; //server name to be sent to client
    int server_number = 12;

    int sockfd = setup_socket();

    printf("DEBUG: *SERVER START*\n");

    printf("DEBUG: socket created.\n");
    printf("DEBUG: waiting for connections...\n");

    //KILL ZOMBIES!!! <- not familiar with this code (it's from Beej's guide... reminder to learn more about it later)
    struct sigaction sa;
    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &sa, NULL) == -1) {
        printf("\e[1;31mERROR: sigaction() returned -1\e[0;37m\n");
    }

    //loop and accept connections when they come
    struct sockaddr_storage client_addr; //used to store address info for client
    socklen_t client_socket_size = sizeof client_addr;
    int clientfd; //ID for client socket
    while(1) {

        //accept blocks until a connection comes in
        clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_socket_size);
        if(clientfd == -1) {
            printf("\e[1;31mERROR: accept() returned -1\e[0;37m\n");
            continue;
        }

        //debug message: print address of who connected
        char s[INET6_ADDRSTRLEN]; 
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*)&client_addr), s, sizeof s);
        //printf("DEBUG: got connection from %s\n", s); //commenting this out bc it prints IP address LOL
        printf("DEBUG: got connection from client\n");

        if(!fork()) {
            //child process 
            char msg[MAX_MESSAGE_SIZE];
            int numbytes = recv(clientfd, msg, MAX_MESSAGE_SIZE-1, 0);
            if(numbytes == -1) {
                printf("\e[1;31mERROR: recv() returned -1\e[0;37m\n");
                exit(1);
            }
            msg[numbytes] = '\0'; //null terminate the message so its a string lol
            printf("DEBUG: received message from client: %s\n", msg);

            char* client_name;
            int client_number;
            const char delim[2] = "\n";

            client_name = strtok(msg, delim);
            client_number = atoi(strtok(NULL, delim));

            //make sure client_number is valid
            if(client_number < 1 || client_number > 100) {
                printf("DEBUG: client sent invalid number: %d\n", client_number);
                printf("\e[1;31mERROR: CLIENT SENT INVALID NUMBER! Terminating server...\e[0;37m\n");
                kill(getppid(), SIGTERM); //tell parent process to terminate
                exit(2);
            }

            printf("\e[1;32mclient name  : %s\n\e[0;37m", client_name);
            printf("\e[1;32mserver name  : %s\n\e[0;37m", server_name);
            printf("\e[1;32mclient number: %d\n\e[0;37m", client_number);
            printf("\e[1;32mserver number: %d\n\e[0;37m", server_number);
            printf("\e[1;32mnumber sum   : %d\n\e[0;37m", server_number+client_number);

            char msg_string[MAX_MESSAGE_SIZE];
            snprintf(msg_string, MAX_MESSAGE_SIZE, "%s\n%d", server_name, server_number);
            printf("DEBUG: sending message: %s\n", msg_string);
            if(send(clientfd, msg_string, strlen(msg_string), 0) == -1) {
                printf("\e[1;31mERROR: send() returned -1\e[0;37m\n");
                exit(3);
            }

            //close sockets
            close(sockfd);
            close(clientfd);
            printf("DEBUG: closing sockets...\n");
            printf("DEBUG: waiting for connections...\n");
            exit(0);
        }

        //parent process
        close(clientfd); //parent doesn't need client socket

    }

    return 0;
}

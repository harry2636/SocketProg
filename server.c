/*
** server.c -- a stream socket server demo
Overall network structure codes are derived from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html which is introduced int the Lab slide.
I implemented Caesar Cipher algorithm with suggested header transfer algorithm.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "packet.h"

#define ARGS 3 // number of command line arguments

#define BACKLOG 200     // how many pending connections queue will hold


/* function from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

/* part of the codes are based on network connecting structure code from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
int main(int argc, char *argv[])
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    int numbytes;
    uint16_t checksum = 0;
    char *port = malloc(strlen(argv[2]) + 1);

    //char recv_buffer[MAX_MESSAGE_SIZE+1];
    //char send_buffer[MAX_MESSAGE_SIZE+1];
    char *recv_buffer = malloc(MAX_MESSAGE_SIZE+1);

    if (argc != ARGS) {
        fprintf(stderr,"example: ./server -p 3123\n");
        exit(1);
    }

    if (!strcmp(argv[1], "-p")) {
        memcpy(port, argv[2], strlen(argv[2]) + 1);
        printf("port: %s\n", port);
    }
    else {
        perror("Invalid parameter option");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
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

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            while(1){
                if ((numbytes = recv_large(new_fd, recv_buffer)) == -1){
                    perror("recv");
                    break;
                }                
                else if (numbytes == 0){
                    printf("zero recv, close connection\n");
                    break;
                }
                //printf("numbytes:%d\n", numbytes);

                //debug_message(recv_buffer);
                

                int ori_checksum = *(uint16_t *)(&recv_buffer[2]);

                checksum = 0;
                memcpy(&recv_buffer[2], &checksum, sizeof(uint16_t));
                if (ori_checksum != checksum1(recv_buffer, numbytes)){
                    perror("checksum");
                    break;
                }

                char *addr = &recv_buffer[HEADER_BYTES];
                uint8_t op = recv_buffer[0];
                uint8_t shift = recv_buffer[1];

                int i;
                for (i = 0; i < numbytes - 8; i++) {
                    if (op == 0) {
                        addr[0] = caesar_encrypt(addr[0], shift);
                    }
                    else if (op == 1) {
                        addr[0] = caesar_decrypt(addr[0], shift);
                    }
                    
                    addr++;

                }



                checksum = checksum1(recv_buffer, numbytes);
                memcpy(&recv_buffer[2], &checksum, sizeof(uint16_t));
                

                //debug_message(recv_buffer);

                if (send(new_fd, recv_buffer, numbytes, 0) == -1){
                    perror("send");
                    break;
                }
                

            }
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }
    
    close(sockfd);
    return 0;
}
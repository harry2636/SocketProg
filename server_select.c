/*
** selectserver.c -- a cheezy multiperson chat server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include "packet.h"

#define ARGS 3 // number of command line arguments

#define BACKLOG 200     // how many pending connections queue will hold

/* part of the codes are based on network connecting structure code from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
int main(int argc, char *argv[])
{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char *recv_buffer = malloc(MAX_MESSAGE_SIZE+1);
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, rv;
    char *port = malloc(strlen(argv[2]) + 1);

    struct addrinfo hints, *ai, *p;
    uint16_t checksum = 0;

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

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {

                    // handle data from a client
                    if ((nbytes = recv_large(i, recv_buffer)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: close connection in socket %d\n", i);
                        }
                        else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    }
                    else {

                        //debug_message(recv_buffer);

                        int ori_checksum = *(uint16_t *)(&recv_buffer[2]);

                        checksum = 0;
                        memcpy(&recv_buffer[2], &checksum, sizeof(uint16_t));

                        if (ori_checksum != checksum1(recv_buffer, nbytes)){
                            perror("checksum");
                            close(i); // bye!
                            FD_CLR(i, &master); // remove from master set
                        }

                        char *addr = &recv_buffer[HEADER_BYTES];
                        uint8_t op = recv_buffer[0];
                        uint8_t shift = recv_buffer[1];

                        int iter;
                        for (iter = 0; iter < nbytes - 8; iter++) {
                            if (op == 0) {
                                addr[0] = caesar_encrypt(addr[0], shift);
                            }
                            else if (op == 1) {
                                addr[0] = caesar_decrypt(addr[0], shift);
                            }
                            
                            addr++;

                        }

                        checksum = checksum1(recv_buffer, nbytes);
                        memcpy(&recv_buffer[2], &checksum, sizeof(uint16_t));

                        //debug_message(recv_buffer);

                        if (send(i, recv_buffer, nbytes, 0) == -1){
                            perror("send");
                            close(i); // bye!
                            FD_CLR(i, &master); // remove from master set
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}
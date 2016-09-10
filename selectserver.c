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

#define PORT "3000"   // port we're listening on

#define MAX_MESSAGE_SIZE (10 * (1<<20)) // max number of bytes we can get at once, should be 10MB

#define HEADER_BYTES 8

/* checksum calculting function from http://locklessinc.com/articles/tcp_checksum/ which is introduced in the Lab slide */
unsigned short checksum1(const char *buf, unsigned size)
{
    unsigned sum = 0;
    int i;

    /* Accumulate checksum */
    for (i = 0; i < size - 1; i += 2)
    {
        unsigned short word16 = *(unsigned short *) &buf[i];
        sum += word16;
    }

    /* Handle odd-sized case */
    if (size & 1)
    {
        unsigned short word16 = (unsigned char) buf[i];
        sum += word16;
    }

    /* Fold to get the ones-complement result */
    while (sum >> 16) sum = (sum & 0xFFFF)+(sum >> 16);

    /* Invert to get the negative in ones-complement arithmetic */
    return ~sum;
}

void debug_message(const char *buf){
    printf("operator: %hhu\n", buf[0]);
    printf("shift: %hhu\n", buf[1]);
    printf("checksum: %hu\n", *(uint16_t *)(&buf[2]));
    printf("length: %u\n", ntohl( *(uint32_t *) (&buf[4])));
    //printf("data: %s\n\n", &buf[HEADER_BYTES]);
    int i=0;
    uint32_t length = ntohl( *(uint32_t *) (&buf[4]));
    length -= 8;
    uint32_t ori_length = length;
    while (length){
        printf("Printed char value\n");
        printf("%hu\n", (unsigned short)buf[HEADER_BYTES + ori_length - length] );
        length--;
    }

}

char caesar_encrypt(char c, uint8_t shift){
    c = tolower(c);
    if (c >= 97 && c + shift <= 122) {
        c += shift;
    }
    else if (c + shift > 122 && c <= 122) {
        uint8_t remain = shift - (122 - c + 1);
        c = 'a' + remain;
    }
    return c;
}

char caesar_decrypt(char c, uint8_t shift){
    c = tolower(c);
    if (c <=122 && c - shift >= 97) {
        c -= shift;
    }
    else if (c - shift < 97 && c >= 97) {
        uint8_t remain = shift - (c - 97 +1);
        c = 'z' - shift;
    }

    return c;
}

/* function from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* part of the codes are based on network connecting structure code from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
int main(void)
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
    int i, j, rv;

    struct addrinfo hints, *ai, *p;
    uint16_t checksum = 0;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
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
    if (listen(listener, 10) == -1) {
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
                    int new_fd = i;
                    // handle data from a client
                    if ((nbytes = recv(i, recv_buffer, MAX_MESSAGE_SIZE, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {

                        int ori_checksum = *(uint16_t *)(&recv_buffer[2]);

                        checksum = 0;
                        memcpy(&recv_buffer[2], &checksum, sizeof(uint16_t));
                        //printf("before checksum\n");
                        if (ori_checksum != checksum1(recv_buffer, nbytes)){
                            perror("checksum");
                            break;
                        }

                        char *addr = &recv_buffer[HEADER_BYTES];
                        uint8_t op = recv_buffer[0];
                        uint8_t shift = recv_buffer[1];

                        int i;
                        for (i = 0; i < nbytes - 8; i++) {
                            if (op == 0) {
                                addr[0] = caesar_encrypt(addr[0], shift);
                            }
                            else if (op == 1) {
                                addr[1] = caesar_decrypt(addr[0], shift);
                            }
                            
                            addr++;

                        }

                        checksum = checksum1(recv_buffer, nbytes);
                        memcpy(&recv_buffer[2], &checksum, sizeof(uint16_t));
                        
                        //memcpy(&send_buffer, &recv_buffer, nbytes+1);

                        debug_message(recv_buffer);

                        if (send(new_fd, recv_buffer, nbytes, 0) == -1){
                            perror("send");
                            break;
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}
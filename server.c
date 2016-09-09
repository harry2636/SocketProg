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

#define PORT "3000"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

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
    printf("checksum: %hu\n", *(uint16_t *)(&buf[2]) );
    printf("length: %u\n", ntohl( *(uint32_t *) (&buf[4])));
    printf("data: %s\n\n", &buf[HEADER_BYTES]);

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



void sigchld_handler(int s)
{
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
    int numbytes;
    uint16_t checksum = 0;

    //char recv_buffer[MAX_MESSAGE_SIZE+1];
    //char send_buffer[MAX_MESSAGE_SIZE+1];
    char *recv_buffer = malloc(MAX_MESSAGE_SIZE+1);


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
                if ((numbytes = recv(new_fd, recv_buffer, MAX_MESSAGE_SIZE, 0)) == -1){
                    perror("recv");
                    break;
                }
                printf("numbytes:%d\n", numbytes);
                recv_buffer[numbytes] = '\0';
                printf("server received: %s\n", &recv_buffer[HEADER_BYTES]);

                debug_message(recv_buffer);

                char c = recv_buffer[HEADER_BYTES];
                char *addr = &recv_buffer[HEADER_BYTES];
                uint8_t op = recv_buffer[0];
                uint8_t shift = recv_buffer[1];

                while(addr[0]!= '\0'){
                    if (op == 0) {
                        addr[0] = caesar_encrypt(addr[0], shift);
                    }
                    else if (op == 1) {
                        addr[1] = caesar_decrypt(addr[0], shift);
                    }
                    
                    addr++;

                }

                checksum = 0;
                memcpy(&recv_buffer[2], &checksum, sizeof(uint16_t));

                checksum = checksum1(recv_buffer, HEADER_BYTES + strlen(&recv_buffer[HEADER_BYTES]));
                memcpy(&recv_buffer[2], &checksum, sizeof(uint16_t));
                
                //memcpy(&send_buffer, &recv_buffer, HEADER_BYTES + strlen(&recv_buffer[HEADER_BYTES])+1);

                debug_message(recv_buffer);

                if (send(new_fd, recv_buffer, HEADER_BYTES + strlen(&recv_buffer[HEADER_BYTES]), 0) == -1){
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
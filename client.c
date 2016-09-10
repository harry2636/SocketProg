/*
** client.c -- a stream socket client demo
Overall network structures codes are derived from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html which is introduced in the Lab slide.
I implemented Caesar Cipher algorithm with suggested header transfer algorithm.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "3000" // the port client will be connecting to 

#define MAX_MESSAGE_SIZE (10 * (1<<20)) // max number of bytes we can get at once, should be 10MB

#define HEADER_BYTES 8

/* checksum calculting function from http://locklessinc.com/articles/tcp_checksum/ which is introduced the in Lab slide */
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

/* Modified fgets implementation in The C Programming Language.(p165, 2nd ed.)
I found it in http://stackoverflow.com/questions/16397832/fgets-implementation-kr */

uint32_t new_fgets(char* s, int n, FILE *iop)
{
    register int c;
    register char* cs;
    cs = s;
    uint32_t count = 0;
    while(--n > 0 && (c = getc(iop)) != EOF)
    {
        count++;
        //printf("fgets c: %c !\n", c);
        //printf("count :%d\n", count);

    if((*cs++ = c) == '\n')
        break;          
    }

    *cs = '\0';
    //return (c == EOF && cs == s) ? NULL : s;
    return count;
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
int main(int argc, char *argv[])
{
    int sockfd, numbytes;  
    //char send_buffer[MAX_MESSAGE_SIZE+1];
    //char recv_buffer[MAX_MESSAGE_SIZE+1];
    char *send_buffer = malloc(MAX_MESSAGE_SIZE+1);
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    uint8_t op = 0;
    uint8_t shift = 3;
    uint16_t checksum = 0;
    uint32_t length = htonl(0);

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memcpy(&send_buffer[0], &op, sizeof(uint8_t));
    memcpy(&send_buffer[1], &shift, sizeof(uint8_t));
    //memcpy(&send_buffer[2], &checksum, sizeof(uint16_t));
    //memcpy(&send_buffer[4], &length, sizeof(int));


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
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

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    //printf("client: connecting to %s\n", s);

    while(1){
        uint32_t count = new_fgets(&send_buffer[HEADER_BYTES], MAX_MESSAGE_SIZE + 1, stdin);
        if (count == 0){
            //perror("fget");
            exit(0);
        }
        /*
        int i = 0;
        while (send_buffer[HEADER_BYTES + i] != '\n' && send_buffer[HEADER_BYTES + i] != EOF){
            i++;
        }
        if (send_buffer[HEADER_BYTES + i] == '\n'){
            i++;
        }
        int buf_len = i;
        */
        int buf_len = count;
        /*
        if(send_buffer[HEADER_BYTES + buf_len-1] == '\n'){
            send_buffer[HEADER_BYTES + buf_len-1] = '\0';
        }
        */
        //printf("sending: %s\n", &send_buffer[HEADER_BYTES]);


        length = htonl(HEADER_BYTES + buf_len);
        memcpy(&send_buffer[4], &length, sizeof(int));

        checksum = 0;
        memcpy(&send_buffer[2], &checksum, sizeof(uint16_t));

        checksum = checksum1(send_buffer, HEADER_BYTES + buf_len);
        memcpy(&send_buffer[2], &checksum, sizeof(uint16_t));

        //debug_message(send_buffer);
        //printf("==================\n");

        if ((numbytes = send(sockfd, send_buffer, HEADER_BYTES + buf_len, 0 )) == -1) {
            perror("send");
            exit(1);
        }

        if ((numbytes = recv(sockfd, send_buffer, MAX_MESSAGE_SIZE, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        numbytes -= 8;
        int ori_numbytes = numbytes;
        while(numbytes != 0){
            //printf("strlen:%lu\n", strlen(&send_buffer[HEADER_BYTES]));
            printf("%c", send_buffer[HEADER_BYTES + ori_numbytes - numbytes]);
            numbytes--;
        }


        //printf("%c", &send_buffer[HEADER_BYTES]);

        //debug_message(send_buffer);
    }
    freeaddrinfo(servinfo); // all done with this structure
    close(sockfd);

    return 0;
}

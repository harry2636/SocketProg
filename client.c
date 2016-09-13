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
#include "packet.h"


#define ARGS 9 // number of command line arguments



/* Modified clibrary fgets function implementation in The C Programming Language.(p165, 2nd ed.)
I found it in http://stackoverflow.com/questions/16397832/fgets-implementation-kr */
uint32_t new_fgets(char* s, int n, FILE *iop)
{
    register int c;
    register char* cs;
    cs = s;
    uint32_t count = 0;
    while (--n > 0 && (c = getc(iop)) != EOF) {
        count++;
        //printf("fgets c: %c !\n", c);
        //printf("count :%d\n", count);

        if ((*cs++ = c) == '\n') {
            break;
        }

        if (count == 1000) {
            cs++;
            break;
        }
    }

    *cs = '\0';
    //return (c == EOF && cs == s) ? NULL : s;
    return count;
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
    char *hostaddr, *port;

    uint8_t op = 0;
    uint8_t shift = 0;
    uint16_t checksum = 0;
    uint32_t length = htonl(0);

    char h_flag = 0, p_flag = 0, o_flag =0, s_flag = 0;         

    if (argc != ARGS) {
        fprintf(stderr,"example: ./client –h 143.248.111.222 –p 1234 –o 0 –s 5\n");
        exit(1);
    }

    int i = 0;
    for (i = 1; i < ARGS; i = i + 2){
        //printf("argv[i]:%s, cmp to:%s, %d\n", argv[i], "-h", strcmp(argv[i], "-h"));
        if (!strcmp(argv[i], "-h")){
            if (h_flag == 1) {
                perror("Duplicated parameter");
                exit(1);
            }
            memcpy(&hostaddr, &argv[i+1], sizeof(argv[i+1]));
            //fprintf(stdout, "-h\n");
            h_flag = 1;
        }
        else if (!strcmp(argv[i], "-p")){
            if (p_flag == 1) {
                perror("Duplicated parameter");
                exit(1);
            }
            memcpy(&port, &argv[i+1], sizeof(argv[i+1]));
            //fprintf(stdout, "-p\n");
            p_flag = 1;
        }
        else if (!strcmp(argv[i], "-o")){
            if (o_flag == 1) {
                perror("Duplicated parameter");
                exit(1);
            }
            op = atoi(argv[i+1]);
            if (op != 0 && op != 1) {
                perror("Invalid op value");
                exit(1);
            }
            //fprintf(stdout, "-o\n");
            o_flag = 1;
        }
        else if (!strcmp(argv[i], "-s")){
            if (s_flag == 1) {
                perror("Duplicated parameter");
                exit(1);
            }
            int ori_shift = atoi(argv[i+1]);

            
            if (ori_shift < 0) {
                perror("Invalid shift value");
                exit(1);
            }

            if (ori_shift >= 26) {
                ori_shift %=  26;
            }

            
            shift = (uint8_t)ori_shift;

            s_flag = 1;
        }
        else {
            fprintf(stderr, "Invalid parameter option in %dth param: %s\n", i, argv[i]);
            exit(1);
        }


    }

    memcpy(&send_buffer[0], &op, sizeof(uint8_t));
    memcpy(&send_buffer[1], &shift, sizeof(uint8_t));
    //memcpy(&send_buffer[2], &checksum, sizeof(uint16_t));
    //memcpy(&send_buffer[4], &length, sizeof(int));


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostaddr, port, &hints, &servinfo)) != 0) {
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

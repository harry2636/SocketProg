#include "packet.h"


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

/* function from http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void debug_message(const char *buf){
    printf("operator: %hhu\n", buf[0]);
    printf("shift: %hhu\n", buf[1]);
    printf("checksum: %hu\n", *(uint16_t *)(&buf[2]));
    printf("length: %u\n", ntohl( *(uint32_t *) (&buf[4])));
    //printf("data: %s\n\n", &buf[HEADER_BYTES]);
    uint32_t length = ntohl( *(uint32_t *) (&buf[4]));
    length -= 8;
    /*
    uint32_t ori_length = length;
    while (length){
        printf("Printed char value\n");
        printf("%hu\n", (unsigned short)buf[HEADER_BYTES + ori_length - length] );
        length--;
    }
    */

}

char caesar_encrypt(char c, uint8_t shift){
    c = tolower(c);
    if (shift >= 26) {
        shift = shift % 26;
        //printf("shift:%hhu\n", shift);
    }
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
    if (shift >= 26) {
        shift = shift % 26;
    }
    if (c <=122 && c - shift >= 97) {
        c -= shift;
    }
    else if (c - shift < 97 && c >= 97) {
        uint8_t remain = shift - (c - 97 +1);
        c = 'z' - remain;
    }

    return c;
}

int recv_large(int sockfd, char *buf){
	int numbytes;
    if ((numbytes = recv(sockfd, buf, 8, 0)) == -1) {
        return -1;
    }
    else if (numbytes == 0) {
    	return 0;
    }

    int length_receive = ntohl( *(uint32_t *) (&buf[4]));
    length_receive -= numbytes;
    int offset = numbytes;

    while(length_receive) {
        numbytes = recv(sockfd, buf + offset, 1000, 0);
        //printf("In_loop numbytes_recv:%d\n", numbytes);
        if (numbytes > 0) {
            length_receive -= numbytes;
            offset += numbytes;
        }
        else {
        	return -1;
        }
    }
    return offset;
}




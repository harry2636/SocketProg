#ifndef PACKET_H
#define PACKET_H

#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define MAX_MESSAGE_SIZE (10 * (1<<20)) // max number of bytes we can get at once, should be 10MB

#define HEADER_BYTES 8

unsigned short checksum1(const char *buf, unsigned size);

void debug_message(const char *buf);

char caesar_encrypt(char c, uint8_t shift);

char caesar_decrypt(char c, uint8_t shift);

void *get_in_addr(struct sockaddr *sa);

int recv_large(int sockfd, char *buf);

#endif
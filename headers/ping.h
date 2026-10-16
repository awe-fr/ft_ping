#ifndef PING_H
# define PING_H

# include <sys/socket.h>
# include <netinet/ip_icmp.h>
# include <netdb.h>
# include <arpa/inet.h>

# include <sys/time.h>
# include <stdlib.h>
# include <stdint.h>
# include <string.h>
# include <unistd.h>
# include <stdio.h>

typedef struct icmp_packet {
    struct icmphdr  header;
    struct timeval  tv;
    char            pad[40];
} icmp_packet;

void init_ping();
void send_ping(int sockfd, struct addrinfo *res);
icmp_packet create_icmp_packet(int sockfd, struct addrinfo *res);
uint16_t create_checksum(void *packet, int len);

void cleanup(int sockfd, struct addrinfo *res);

void listen_for_replies(int sockfd, struct addrinfo *res);
void print_response(size_t packet_size, char *ip_str, int sequence, int ttl, double rtt_ms);
#endif
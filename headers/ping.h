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
# include <signal.h>
# include <stdio.h>
# include <errno.h>
# include <math.h>

typedef struct icmp_packet {
    struct icmphdr  header;
    struct timeval  tv;
    char            pad[40];
} icmp_packet;

char *check_options(int argc, char **argv);
void check_signal(int signum);

void init_ping(char *hostname);
void send_ping(int sockfd, struct addrinfo *res);
icmp_packet create_icmp_packet(int sockfd, struct addrinfo *res);
uint16_t create_checksum(void *packet, int len);

void cleanup(int sockfd, struct addrinfo *res);

void listen_for_replies(int sockfd, struct addrinfo *res);
void manage_echo_reply(int sockfd, struct addrinfo *res, ssize_t packet_size, icmp_packet *icmp_reply, struct ip *ip_hdr, char *ip_str);
void manage_time_exceeded(int sockfd, struct addrinfo *res, char *buffer, ssize_t ip_len, ssize_t packet_size, char *ip_str);
void manage_dest_unreach(int sockfd, struct addrinfo *res, char *buffer, ssize_t ip_len, ssize_t packet_size, char *ip_str);

void print_usage();
void print_help();
void print_header(int sockfd, struct addrinfo *res, char *hostname);
void print_statistics(char *hostname);
void print_verbose_error(int sockfd, struct addrinfo *res, char *buffer, ssize_t ip_len);
void print_hdr_dump(void *packet, int len);
void print_hdr_tab(int sockfd, struct addrinfo *res, void *packet);

#endif
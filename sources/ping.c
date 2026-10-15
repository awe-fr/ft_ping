#include "./../headers/ping.h"

uint16_t count = 0;

void init_ping() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); // check value
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    struct addrinfo *res;

    int ret = getaddrinfo("8.8.8.8", NULL, &hints,  &res); // dont forget to freeaddrinfo() and check value
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    send_ping(sockfd, res->ai_addr, res->ai_addrlen);

    cleanup(sockfd, res);
}

void send_ping(int sockfd, const struct sockaddr *dest_addr, socklen_t addrlen) {
    icmp_packet packet = create_icmp_packet();

    ssize_t byte_sended = sendto(sockfd, &packet, sizeof(icmp_packet), 0, dest_addr, addrlen); // check value maybe use flag MSG_DONTROUTE for -r
}

icmp_packet create_icmp_packet() {
    icmp_packet packet;
    
    count++;

    memset(&packet, 0, sizeof(packet));

    packet.header.type = ICMP_ECHO;
    packet.header.code = 0;
    packet.header.checksum = 0;
    packet.header.un.echo.id = htons(getpid() & 0xFFFF);
    packet.header.un.echo.sequence = htons(count);
    
    gettimeofday(&packet.tv, NULL); // check value

    packet.header.checksum = create_checksum(&packet, sizeof(icmp_packet));

    return packet;
}

uint16_t create_checksum(void *packet, int len) {
    uint16_t *p = (uint16_t *)packet;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *p;
        p++;
        len -= 2;
    }
   
    if (len == 1) {
        sum += *(uint8_t *)p;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum = (sum >> 16) + (sum & 0xFFFF);

    return (uint16_t)(~sum);
}

void cleanup(int sockfd, struct addrinfo *res) {
    freeaddrinfo(res);
    close(sockfd);
}
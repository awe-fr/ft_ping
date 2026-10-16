#include "./../headers/ping.h"

uint16_t count = 0;

void init_ping() {
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
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

    int ret = getaddrinfo("8.8.8.8", NULL, &hints,  &res);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    send_ping(sockfd, res);

    listen_for_replies(sockfd, res);

    cleanup(sockfd, res);
}

void send_ping(int sockfd, struct addrinfo *res) {
    icmp_packet packet = create_icmp_packet(sockfd, res);

    ssize_t byte_sended = sendto(sockfd, &packet, sizeof(icmp_packet), 0, res->ai_addr, res->ai_addrlen); // check value maybe use flag MSG_DONTROUTE for -r
}

icmp_packet create_icmp_packet(int sockfd, struct addrinfo *res) {
    icmp_packet packet;
    
    count++;

    memset(&packet, 0, sizeof(packet));

    packet.header.type = ICMP_ECHO;
    packet.header.code = 0;
    packet.header.checksum = 0;
    packet.header.un.echo.id = htons(getpid() & 0xFFFF);
    packet.header.un.echo.sequence = htons(count);
    
    if (gettimeofday(&packet.tv, NULL) == -1) {
        perror("gettimeofday");
        cleanup(sockfd, res);
        exit(EXIT_FAILURE);
    }

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

void listen_for_replies(int sockfd, struct addrinfo *res) {
    char buffer[1024];

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval interval;

    interval.tv_sec = 1;
    interval.tv_usec = 0;

    int select_ret = select(sockfd + 1, &readfds, NULL, NULL, &interval);
    if (select_ret < 0) {
        perror("select");
        cleanup(sockfd, res);
        exit(EXIT_FAILURE);
    } else if (select_ret > 0) {
        memset(buffer, 0, sizeof(buffer));

        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(struct sockaddr_in);

        ssize_t recvfrom_ret = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, &from_len);
        if (recvfrom_ret < 0) {
            perror("recvfrom");
            cleanup(sockfd, res);
            exit(EXIT_FAILURE);
        }

        char ip_str[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET, &from_addr.sin_addr, ip_str, sizeof(ip_str)) == NULL) {
            perror("inet_ntop");
            cleanup(sockfd, res);
            exit(EXIT_FAILURE);
        }

        struct ip *ip_hdr = (struct ip *)buffer;
        int ip_len = ip_hdr->ip_hl * 4;

        uint16_t check_checksum = create_checksum(buffer + ip_len, recvfrom_ret - ip_len);
        if (check_checksum != 0) {
            fprintf(stderr, "Checksum error: received packet is corrupted\n");
            cleanup(sockfd, res);
            exit(EXIT_FAILURE);
        }

        icmp_packet *icmp_reply = (icmp_packet *)(buffer + ip_len);

        struct timeval ms_diff;
        if (gettimeofday(&ms_diff, NULL) == -1) {
            perror("gettimeofday");
            cleanup(sockfd, res);
            exit(EXIT_FAILURE);
        }
        timersub(&ms_diff, &icmp_reply->tv, &ms_diff);

        double rtt_ms = ms_diff.tv_sec * 1000.0 + ms_diff.tv_usec / 1000.0;

        print_response(recvfrom_ret - ip_len, ip_str, ntohs(icmp_reply->header.un.echo.sequence), ip_hdr->ip_ttl, rtt_ms);
    }

    
}

void print_response(size_t packet_size, char *ip_str, int sequence, int ttl, double rtt_ms) {
        printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.2f ms\n", packet_size, ip_str, sequence, ttl, rtt_ms);
}

void cleanup(int sockfd, struct addrinfo *res) {
    freeaddrinfo(res);
    close(sockfd);
}
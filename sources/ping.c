#include "./../headers/ping.h"

bool verbose = false;

int transmitted_count = 0;
int received_count = 0;

uint16_t seq_count = 0;

double rtt_min = 0.0;
double rtt_max = 0.0;
double rtt_avg = 0.0;
double rrt_stddev = 0.0; 


bool is_running = true;

char *check_options(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-", 1) == 0) {
            if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
                verbose = true;
            }
        } else {
            return argv[i];
        }
    }
    return NULL;
}

void check_signal(int signum) {
    if (signum == SIGINT) {
        is_running = false;
    }
}

void init_ping(char *hostname) {
    struct sigaction sa;
    sa.sa_handler = check_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

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

    int ret = getaddrinfo(hostname, NULL, &hints,  &res);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    print_header(sockfd, res, hostname);

    while (is_running) {
        send_ping(sockfd, res);
        listen_for_replies(sockfd, res);
    }

    print_statistics(hostname);
    cleanup(sockfd, res);
}

void send_ping(int sockfd, struct addrinfo *res) {
    icmp_packet packet = create_icmp_packet(sockfd, res);

    ssize_t byte_sended = sendto(sockfd, &packet, sizeof(icmp_packet), 0, res->ai_addr, res->ai_addrlen);
    if (byte_sended < 0) {
        perror("sendto");
        cleanup(sockfd, res);
        exit(EXIT_FAILURE);
    }
    transmitted_count++;
}

icmp_packet create_icmp_packet(int sockfd, struct addrinfo *res) {
    icmp_packet packet;
    
    memset(&packet, 0, sizeof(packet));
    
    packet.header.type = ICMP_ECHO;
    packet.header.code = 0;
    packet.header.checksum = 0;
    packet.header.un.echo.id = htons(getpid() & 0xFFFF);
    packet.header.un.echo.sequence = htons(seq_count);

    seq_count++;
    
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

    struct timeval check_timer;

    struct timeval interval;

    interval.tv_sec = 1;
    interval.tv_usec = 0;
    while (((interval.tv_sec) * 1000.0 + (interval.tv_usec) / 1000.0) > 0) {
        int select_ret = select(sockfd + 1, &readfds, NULL, NULL, &interval);
        if (select_ret < 0) {
            if (errno == EINTR){
                return;
            }
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
                continue;
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
            
            rtt_avg += rtt_ms;
            if (rtt_min == 0.0 || rtt_ms < rtt_min) {
                rtt_min = rtt_ms;
            }
            if (rtt_max == 0.0 || rtt_ms > rtt_max) {
                rtt_max = rtt_ms;
            }
            rrt_stddev += rtt_ms * rtt_ms;
            
            print_response(recvfrom_ret - ip_len, ip_str, ntohs(icmp_reply->header.un.echo.sequence), ip_hdr->ip_ttl, rtt_ms);
            received_count++;
        }

        if (gettimeofday(&check_timer, NULL) == -1) {
            perror("gettimeofday");
            cleanup(sockfd, res);
            exit(EXIT_FAILURE);
        }
    }
}

void print_response(size_t packet_size, char *ip_str, int sequence, int ttl, double rtt_ms) {
        printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n", packet_size, ip_str, sequence, ttl, rtt_ms);
}

void print_header(int sockfd, struct addrinfo *res, char *hostname) {
    char ip_str[50];
    if (inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip_str, sizeof(ip_str)) == NULL) {
        perror("inet_ntop");
        cleanup(sockfd, res);
        exit(EXIT_FAILURE);
    }

    if (verbose == true) {
        printf("PING %s (%s): %d data bytes, id 0x%04x = %d\n", hostname, ip_str, (int)sizeof(icmp_packet) - (int)sizeof(struct icmphdr), getpid() & 0xFFFF, getpid() & 0xFFFF);
    } else {
        printf("PING %s (%s): %d data bytes\n", hostname, ip_str, (int)sizeof(icmp_packet) - (int)sizeof(struct icmphdr));
    }
}

void cleanup(int sockfd, struct addrinfo *res) {
    freeaddrinfo(res);
    close(sockfd);
}

void print_statistics(char *hostname) {
    printf("--- %s ping statistics ---\n", hostname);
    if (transmitted_count > 0) {
        printf("%d packets transmitted, %d packets received, %.1f%% packet loss\n", transmitted_count, received_count, ((double)(transmitted_count - received_count) / (double)transmitted_count) * 100.0);
        printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n", rtt_min, rtt_avg / received_count, rtt_max, sqrt((rrt_stddev / received_count) - ((rtt_avg / received_count) * (rtt_avg / received_count))));
    } else {
        printf("%d packets transmitted, %d packets received, 0%% packet loss\n", transmitted_count, received_count);
        printf("round-trip min/avg/max/stddev = 0.000/0.000/0.000/0.000 ms\n");
    }
}
#include "./../headers/ping.h"

bool echo_request = true; // default to true
int stop_after = -1; // -1 means no limit
double interval = 1.0; // default to 1 second
int ttl_limit = -1; // 1 - 255, -1 means no limit
int timeout = -1; // -1 means no limit
struct timeval timeout_timer; // timer for timeout
int linger_time = -1; // -1 means no limit
bool verbose = false;
bool quiet = false;

int transmitted_count = 0;
int received_count = 0;

uint16_t seq_count = 0;

double rtt_min = 0.0;
double rtt_max = 0.0;
double rtt_avg = 0.0;
double rrt_stddev = 0.0;

bool is_running = true;

char *check_options(int argc, char **argv) {
    int address_index = -1;
    bool skip_next = false;

    for (int i = 1; i < argc; i++) {
        if (skip_next == true) {
            skip_next = false;
            continue;
        }
        if (strncmp(argv[i], "-", 1) == 0) {
            if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
                verbose = true;
            } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
                quiet = true;
            } else if (strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "--help") == 0) {
                print_help();
                exit(EXIT_SUCCESS);
            } else if (strcmp(argv[i], "--usage") == 0) {
                print_usage();
                exit(EXIT_SUCCESS);
            } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
                print_version();
                exit(EXIT_SUCCESS);
            } else if (strcmp(argv[i], "--echo") == 0) {
                echo_request = true;
            } else if (strncmp(argv[i], "--ttl=", 6) == 0) {
                ttl_limit = atoi(argv[i]+6);
                if (ttl_limit <= 0) {
                    printf("ping: option value too small: %d", ttl_limit);
                    exit(EXIT_FAILURE);
                } else if (ttl_limit >= 256) {
                    printf("ping: option value too big: %d", ttl_limit);
                    exit(EXIT_FAILURE);
                }
            } else if (strcmp(argv[i], "-c") == 0 || strncmp(argv[i], "--count=", 8) == 0) {
                if (strcmp(argv[i], "-c") == 0) {
                    if (i + 1 == argc) {
                        printf("ping: option requires an argument -- 'c'\n");
                        printf("Try 'ping --help' or 'ping --usage' for more information.\n");
                        exit(EXIT_FAILURE);
                    }
                    stop_after = atoi(argv[i + 1]);
                    skip_next = true;
                } else {
                    stop_after = atoi(argv[i]+8);
                }
            } else if (strcmp(argv[i], "-i") == 0 || strncmp(argv[i], "--interval=", 11) == 0) {
                if (strcmp(argv[i], "-i") == 0) {
                    if (i + 1 == argc) {
                        printf("ping: option requires an argument -- 'i'\n");
                        printf("Try 'ping --help' or 'ping --usage' for more information.\n");
                        exit(EXIT_FAILURE);
                    }
                    interval = atof(argv[i + 1]);
                    skip_next = true;
                } else {
                    interval = atof(argv[i]+11);
                }
                if (interval < 2.0) {
                    printf("ping: option value too small: %.3f\n", interval);
                    exit(EXIT_FAILURE);
                }
            } else if (strcmp(argv[i], "-w") == 0 || strncmp(argv[i], "--timeout=", 10) == 0) {
                if (strcmp(argv[i], "-w") == 0) {
                    if (i + 1 == argc) {
                        printf("ping: option requires an argument -- 'w'\n");
                        printf("Try 'ping --help' or 'ping --usage' for more information.\n");
                        exit(EXIT_FAILURE);
                    }
                    timeout = atoi(argv[i + 1]);
                    skip_next = true;
                } else {
                    timeout = atoi(argv[i]+10);
                }
                if (timeout <= 0) {
                    printf("ping: option value too small: %d\n", timeout);
                    exit(EXIT_FAILURE);
                }
            } else if (strcmp(argv[i], "-W") == 0 || strncmp(argv[i], "--linger=", 9) == 0) {
                if (strcmp(argv[i], "-W") == 0) {
                    if (i + 1 == argc) {
                        printf("ping: option requires an argument -- 'W'\n");
                        printf("Try 'ping --help' or 'ping --usage' for more information.\n");
                        exit(EXIT_FAILURE);
                    }
                    linger_time = atoi(argv[i + 1]);
                    skip_next = true;
                } else {
                    linger_time = atoi(argv[i]+9);
                }
                if (linger_time <= 0) {
                    printf("ping: option value too small: %d\n", linger_time);
                    exit(EXIT_FAILURE);
                }
            } 
        } else {
            if (address_index == -1) {
                address_index = i;
            }
        }
    }
    if (address_index != -1) {
        return argv[address_index];
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

    if (ttl_limit != -1) {
        if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl_limit, sizeof(ttl_limit))) {
            perror("setsockopt TTL");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
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

    if (timeout != -1) {
        if (gettimeofday(&timeout_timer, NULL) == -1) {
            perror("gettimeofday");
            cleanup(sockfd, res);
            exit(EXIT_FAILURE);
        }
    }

    int i = 0;

    while (is_running && i != stop_after) {
        if (timeout != -1) {
            struct timeval current_time;
            if (gettimeofday(&current_time, NULL) == -1) {
                perror("gettimeofday");
                cleanup(sockfd, res);
                exit(EXIT_FAILURE);
            }
            double elapsed_time = (current_time.tv_sec - timeout_timer.tv_sec) + (current_time.tv_usec - timeout_timer.tv_usec) / 1000000.0;
            if (elapsed_time >= timeout) {
                break;
            }
        }
        send_ping(sockfd, res);
        listen_for_replies(sockfd, res);
        i++;
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

    struct timeval time_interval;

    time_interval.tv_sec = (long int)interval;
    time_interval.tv_usec = (long int)((interval - (long int)interval) * 1000000);
    while (((time_interval.tv_sec) * 1000.0 + (time_interval.tv_usec) / 1000.0) > 0) {
        int select_ret = select(sockfd + 1, &readfds, NULL, NULL, &time_interval);
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

            if (icmp_reply->header.type == ICMP_ECHOREPLY) {
                manage_echo_reply(sockfd, res, recvfrom_ret - ip_len, icmp_reply, ip_hdr, ip_str);
            } else if (icmp_reply->header.type == ICMP_TIME_EXCEEDED) {
                manage_time_exceeded(sockfd, res, buffer, ip_len, recvfrom_ret - ip_len, ip_str);
            } else if (icmp_reply->header.type == ICMP_DEST_UNREACH) {
                manage_dest_unreach(sockfd, res, buffer, ip_len, recvfrom_ret - ip_len, ip_str);
            }

        }

        if (gettimeofday(&check_timer, NULL) == -1) {
            perror("gettimeofday");
            cleanup(sockfd, res);
            exit(EXIT_FAILURE);
        }
    }
}

void manage_echo_reply(int sockfd, struct addrinfo *res, ssize_t packet_size, icmp_packet *icmp_reply, struct ip *ip_hdr, char *ip_str) {
    struct timeval ms_diff;
    if (gettimeofday(&ms_diff, NULL) == -1) {
        perror("gettimeofday");
        cleanup(sockfd, res);
        exit(EXIT_FAILURE);
    }
    timersub(&ms_diff, &icmp_reply->tv, &ms_diff);

    double rtt_ms = ms_diff.tv_sec * 1000.0 + ms_diff.tv_usec / 1000.0;
    
    if (linger_time != -1 && rtt_ms > linger_time * 1000.0) {
        return;
    }

    rtt_avg += rtt_ms;
    if (rtt_min == 0.0 || rtt_ms < rtt_min) {
        rtt_min = rtt_ms;
    }
    if (rtt_max == 0.0 || rtt_ms > rtt_max) {
        rtt_max = rtt_ms;
    }
    rrt_stddev += rtt_ms * rtt_ms;
    
    if (quiet == false) {
        printf("%ld bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n", packet_size, ip_str, ntohs(icmp_reply->header.un.echo.sequence), ip_hdr->ip_ttl, rtt_ms);
    }
    received_count++;
}

void manage_time_exceeded(int sockfd, struct addrinfo *res, char *buffer, ssize_t ip_len, ssize_t packet_size, char *ip_str) {
    printf("%ld bytes from %s: Time to live exceeded\n", packet_size, ip_str);
    if(verbose == true) {
        print_verbose_error(sockfd, res, buffer, ip_len);
    }
}

void manage_dest_unreach(int sockfd, struct addrinfo *res, char *buffer, ssize_t ip_len, ssize_t packet_size, char *ip_str) {
    printf("%ld bytes from %s: Destination Host Unreachable\n", packet_size, ip_str);
    if (verbose == true) {
        print_verbose_error(sockfd, res, buffer, ip_len);
    }
}

void print_usage() {
    printf("Usage: ping [--echo] [-c NUMBER | --count=NUMBER] [-i NUMBER |--interval=NUMBER] [--ttl=N]\n");
    printf("            [-v | --verbose] [-w N | --timeout=N] [-W N | --linger=N] [-q | --quiet] [-? | --help]\n");
    printf("            [--usage] [-V | --version] HOST ...\n");
}

void print_help() {
    printf("Usage: ping [OPTION...] HOST ...\n");
    printf("Send ICMP ECHO_REQUEST packets to network hosts.\n");
    printf("\n");
    printf(" Options controlling ICMP request types:\n");
    printf("      --echo                 send ICMP_ECHO packets (default)\n");
    printf("\n");
    printf(" Options valid for all request types:\n");
    printf("\n");
    printf("  -c, --count=NUMBER         stop after sending NUMBER packets\n");
    printf("  -i, --interval=NUMBER      wait NUMBER seconds between sending each packet\n");
    printf("      --ttl=N                specify N as time-to-live\n");
    printf("  -v, --verbose              verbose output\n");
    printf("  -w, --timeout=N            stop after N seconds\n");  // to implement
    printf("  -W, --linger=N             number of seconds to wait for response\n");  // to implement
    printf("\n");
    printf(" Options valid for --echo requests:\n");
    printf("\n");
    printf("  -q, --quiet                quiet output\n");
    printf("  -?, --help                 give this help list\n");
    printf("      --usage                give a short usage message\n");
    printf("  -V, --version              print program version\n");
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

void print_version() {
    printf("ft_ping version 1.0\n");
}

void cleanup(int sockfd, struct addrinfo *res) {
    freeaddrinfo(res);
    close(sockfd);
}

void print_statistics(char *hostname) {
    printf("--- %s ping statistics ---\n", hostname);
    if (transmitted_count > 0) {
        printf("%d packets transmitted, %d packets received, %.1f%% packet loss\n", transmitted_count, received_count, ((double)(transmitted_count - received_count) / (double)transmitted_count) * 100.0);
        if (received_count > 0) {
            printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n", rtt_min, rtt_avg / received_count, rtt_max, sqrt((rrt_stddev / received_count) - ((rtt_avg / received_count) * (rtt_avg / received_count))));
        }
    } else {
        printf("%d packets transmitted, %d packets received, 0%% packet loss\n", transmitted_count, received_count);
        printf("round-trip min/avg/max/stddev = 0.000/0.000/0.000/0.000 ms\n");
    }
}

void print_verbose_error(int sockfd, struct addrinfo *res, char *buffer, ssize_t ip_len) {
    int original_ip_offset = ip_len + 8;
    struct ip *original_ip_hdr = (struct ip *)(buffer + original_ip_offset);
    int original_ip_len = original_ip_hdr->ip_hl * 4;

    print_hdr_dump(buffer + original_ip_offset, original_ip_len);
    print_hdr_tab(sockfd, res, buffer + original_ip_offset);
}

void print_hdr_dump(void *packet, int len) {
    uint16_t *p = (uint16_t *)packet;

    printf("IP Hdr Dump:\n");
    printf(" ");

    for (int i = 0; i < len/2; i += 1) {
        printf("%04x ", ntohs(p[i]));
    }

    printf("\n");
}

void print_hdr_tab(int sockfd, struct addrinfo *res, void *packet) {
    struct ip *original_ip = (struct ip *)packet;
    int original_ip_len = original_ip->ip_hl * 4;
    icmp_packet *original_packet = (icmp_packet *)(packet + original_ip_len);

    uint16_t flags_offset = ntohs(original_ip->ip_off);
    int flags = flags_offset >> 13;
    int frag_off = flags_offset & 0x1FFF;

    char src_str[INET_ADDRSTRLEN]; 
    char dst_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &original_ip->ip_src, src_str, sizeof(src_str)) == NULL || inet_ntop(AF_INET, &original_ip->ip_dst, dst_str, sizeof(dst_str)) == NULL) {
        perror("inet_ntop");
        cleanup(sockfd, res);
        exit(EXIT_FAILURE);
    }

    printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src      Dst     Data\n");
    printf(" %d  %d  %02x %04x %04x   %d %04x  %02x  %02x %04x %s  %s\n", original_ip->ip_v, original_ip->ip_hl, original_ip->ip_tos, ntohs(original_ip->ip_len), ntohs(original_ip->ip_id), flags, frag_off, original_ip->ip_ttl, original_ip->ip_p, ntohs(original_ip->ip_sum), src_str, dst_str);
    printf("ICMP: type %d, code %d, size %d, id 0x%04x, seq 0x%04x\n", original_packet->header.type, original_packet->header.code, ntohs(original_ip->ip_len) - original_ip_len, ntohs(original_packet->header.un.echo.id), ntohs(original_packet->header.un.echo.sequence));
}
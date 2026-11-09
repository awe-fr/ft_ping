#include "./headers/ping.h"

int main(int argc, char **argv) {
    char *hostname = check_options(argc, argv);
    if (!hostname) {
        print_usage();
        exit(EXIT_FAILURE);
    }
    init_ping(hostname);
    return 0;
}
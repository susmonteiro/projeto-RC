#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/select.h>
#include "config.h"
#include "connection.h"
#include "error.h"

#define MAXARGS 8
#define MINARGS 1

int verbose = FALSE;

int fd_udp, fd_tcp;
socklen_t addrlen_udp, addrlen_tcp;
struct addrinfo hints_udp, *res_udp, hints_tcp, *res_tcp;
struct sockaddr_in addr_udp, addr_tcp;

char fsport[8], asport[8], asip[32];

// print if verbose mode
void printv(char* message) {    // TODO move to common file
    if (verbose == TRUE) printf("%s\n", message);
}

void endFS() {
    freeaddrinfo(res_udp);
    freeaddrinfo(res_tcp);
    close(fd_udp);
    close(fd_tcp);
    printv("Ending FS...\n");
    exit(0);
}



int main(int argc, char* argv[]) {
    int i, n;

    if (argc < MINARGS || argc > MAXARGS) {
        printf("Usage: %s [-q FSport] [-n ASIP] [-p ASport] [-v]\n", argv[0]);
        printError("incorrect number of arguments");
    }
    strcpy(fsport, FSPORT);
    strcpy(asport, ASPORT);
    strcpy(asip, ASIP);

    for (i = MINARGS; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            verbose = TRUE;             // TODO move to common file
        } else if (!strcmp(argv[i], "-q")) {
            strcpy(fsport, argv[++i]);
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        } else if (!strcmp(argv[i], "-n")) {
            strcpy(asip, argv[++i]);
        } else if (!strcmp(argv[i], "-h")) {
            printf("Usage: %s [-q FSport] [-n ASIP] [-p ASport] [-v]\n", argv[0]);
            exit(0);
        }
    }

    udpConnect(asip, asport, &fd_udp, &res_udp);

    printv("udp connection with AS established");

    tcpOpenConnection(FSPORT, &fd_tcp, &res_tcp);
    n = listen(fd_tcp, 5);
    if (n == -1) printError("TCP: listen()");

    printv("tcp connection open");

    signal(SIGINT, endFS);

    return 0;
}
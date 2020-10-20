#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include "config.h"

#define MAXARGS 9
#define MINARGS 1

#define MAX(a, b) a*(a>b) + b*(b>=a)

int fd;
fd_set rset;
struct addrinfo hints, *res;
socklen_t addrlen;
struct sockaddr_in addr;

char fsip[32], fsport[8], asip[32], asport[8];

void errorExit(char* errorMessage) {
    printf("ERROR: %s: %s\n", errorMessage, strerror(errno));
    exit(1);
}

void tcpConnect() {
    int n;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) errorExit("socket()");

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET; // IPv4
    hints.ai_socktype=SOCK_STREAM; // UDP socket

    n = getaddrinfo(asip, asport, &hints, &res);
    if (n != 0) errorExit("getaddrinfo()");

    n = connect(fd, res->ai_addr, res->ai_addrlen);
    if (n != -1) errorExit("connect()");

}

void login(char* uid, char* pass) {
    int n;
    char message[64];

    sprintf(message, "LOG %s %s\n", uid, pass);
    printf("our message: %s", message);
    n = write(fd, message, strlen(message)*sizeof(char));
    if (n == -1) errorExit("write()");
}

void requestFile() {
    char op;
    char* file;
    scanf("%c", &op);
    switch(op) {
        case 'L':
            printf("does list\n");
            break;
        case 'D':
            scanf("%s", file);
            printf("does delete\n");
            break;
        case 'R':
            scanf("%s", file);
            printf("does retrieve\n");
            break;
        case 'U':
            scanf("%s", file);
            printf("does upload\n");
        case 'X':
            printf("does remove\n");
    }
}

void validateCode(char* vc) {

}

int main(int argc, char* argv[]) {
    char command[6], uid[7], pass[10], reply[9], buffer[32], vc[4];
    int n, i, maxfdp1;
    
    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s -n ASIP] [-p ASport] [-m FSIP] [-q FSport]\n", argv[0]);
        errorExit("incorrect number of arguments");
    }

    strcpy(asip, ASIP);
    strcpy(asport, ASPORT);
    strcpy(fsip, FSIP);
    strcpy(fsport, FSPORT);

    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-n")) {
            strcpy(asip, argv[++i]);
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        } else if (!strcmp(argv[i], "-m")) {
            strcpy(fsip, argv[++i]);  
        } else if (!strcmp(argv[i], "-q")) {
            strcpy(fsport, argv[++i]);
        }
    }

    tcpConnect();

    FD_ZERO(&rset); 
    maxfdp1 = MAX(STDIN, fd) + 1;

    while(1) {
        FD_SET(STDIN, &rset);
        FD_SET(fd, &rset);

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd, &rset)) {
            n = read(fd, reply, 9);
            if (n == -1) errorExit("read()");
            reply[n] = '\0';

            printf("server reply: %s", reply); /* debug */ // TODO remove this

            if (!strcmp(reply, "RLO OK\n"))
                printf("You are now logged in\n");
            else if (!strcmp(reply, "RLO NOK\n"))
                printf("Login was a failureeee you a failureeee\n"); // TODO change this
            else if (!strcmp(reply, "RRQ OK\n"))
                printf("Request successful\n"); // TODO change this
            else if (!strcmp(reply, "RRQ NOK\n"))
                printf("Request was a failureeee you a failureeee\n"); // TODO change this
            else
                printf("nope, not working\n");  /* debug */ // TODO remove this
        }
        
        if (FD_ISSET(STDIN, &rset)) {
                scanf("%s", command);
                if (!strcmp(command, "login")) {
                    scanf("%s %s", uid, pass);
                    login(uid, pass);
                } else if (!strcmp(command, "req"))
                    requestFile();
                else if (!strcmp(command, "val")) {
                    scanf("%s", vc);
                    validateCode(vc);
                }
                else printf("ERR1\n");    /* debug */ // TODO remove this
        }
    }

    return 0;
}
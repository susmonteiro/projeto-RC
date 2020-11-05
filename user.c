#include "config.h"
#include "connection.h"
#include "error.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAXARGS 10
#define MINARGS 1

#define MAX(a, b) a *(a > b) + b *(b >= a)

time_t t;

int fd;
fd_set rset;
struct addrinfo hints, *res;
socklen_t addrlen;
struct sockaddr_in addr;

char fsip[32], fsport[8], asip[32], asport[8];
char uid[7], pass[10], vc[4], rid[5], file[32];
char op;

void login() {
    int n;
    char message[64];
    sprintf(message, "LOG %s %s\n", uid, pass);
    //printf("our message: %s", message);   //DEBUG
    n = write(fd, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void requestFile() {
    int n;
    char message[64];

    scanf("%c", &op);
    switch (op) {
    case 'L':
        sprintf(message, "REQ %s %s %c\n", uid, rid, op);
        break;
    case 'D':
        scanf("%s", file);
        sprintf(message, "REQ %s %s %c %s\n", uid, rid, op, file);
        break;
    case 'R':
        scanf("%s", file);
        sprintf(message, "REQ %s %s %c %s\n", uid, rid, op, file);
        break;
    case 'U':
        scanf("%s", file);
        //printf("%c %s\n", op, file); //DEBUG
        //printf("%s %s %c %s\n", uid, rid, op, file); //DEBUG
        sprintf(message, "REQ %s %s %c %s\n", uid, rid, op, file);
        break;
    case 'X':
        sprintf(message, "REQ %s %s %c\n", uid, rid, op);
        break;
    }
    //printf("our message: %s\n", message); //DEBUG
    n = write(fd, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void validateCode() {
    int n;
    char message[64];

    sprintf(message, "AUT %s %s %s\n", uid, rid, vc);
    //printf("%s\n", message); //DEBUG
    n = write(fd, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void endUser() {
    exit(0);
}


/*      === main code ===        */

void fdManager() {
    char command[6], reply[10], acr[4], tid[5];

    int n, maxfdp1;

    while (1) {
        FD_ZERO(&rset);
        FD_SET(STDIN, &rset);
        FD_SET(fd, &rset);

        maxfdp1 = MAX(STDIN, fd) + 1;

        n = select(maxfdp1, &rset, NULL, NULL, NULL);
        if (n == -1) errorExit("select()");

        if (FD_ISSET(STDIN, &rset)) {
            scanf("%s", command);
            if (!strcmp(command, "login")) {
                scanf("%s %s", uid, pass);
                login();
            } else if (!strcmp(command, "req")) {
                scanf("%c", &op);
                requestFile();
            } else if (!strcmp(command, "val")) {
                scanf("%s", vc);
                validateCode();
            } else
                printf("Error: invalid command\n");
        }

        if (FD_ISSET(fd, &rset)) {
            n = read(fd, reply, 10);
            if (n == -1)
                errorExit("read()");
            else if (n == 0) {
                printf("Error: AS closed\n");
                endUser();
            }
            reply[n] = '\0';

            sscanf(reply, "%s %s", acr, tid);
            tid[4] = '\0';

            if (!strcmp(reply, "RLO OK\n"))
                printf("You are now logged in.\n");
            else if (!strcmp(reply, "RLO NOK\n"))
                printf("Login was a failureeee you a failureeee\n"); // TODO change this
            else if (!strcmp(reply, "RRQ OK\n"))
                printf("Request successful\n"); // TODO change this
            else if (!strcmp(reply, "RRQ NOK\n"))
                printf("Request was a failureeee you a failureeee\n"); // TODO change this
            else if (!strcmp(acr, "RAU"))
                printf("Authenticated! (TID=%s)\n", tid);
            else {
                printf("Unknown message from AS\n");
                endUser();
            }
        }

        
    }
}

int main(int argc, char *argv[]) {
    int i;

    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s [-n ASIP] [-p ASport] [-m FSIP] [-q FSport]\n", argv[0]);
        errorExit("incorrect number of arguments");
    }

    strcpy(asip, ASIP);
    strcpy(asport, ASPORT);
    strcpy(fsip, FSIP);
    strcpy(fsport, FSPORT);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            printf("​Usage: %s [-n ASIP] [-p ASport] [-m FSIP] [-q FSport]\n", argv[0]);
            exit(0);
        } else if (!strcmp(argv[i], "-n")) {
            strcpy(asip, argv[++i]);
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        } else if (!strcmp(argv[i], "-m")) {
            strcpy(fsip, argv[++i]);
        } else if (!strcmp(argv[i], "-q")) {
            strcpy(fsport, argv[++i]);
        }
    }

    tcpConnect(asip, asport, &fd, &res);
    sprintf(rid, "%d", rand() % 9999);

    signal(SIGINT, endUser);
    signal(SIGPIPE, endUser);

    fdManager();

    return 0;
}
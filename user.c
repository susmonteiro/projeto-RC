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
#include <time.h>
#include "config.h"
#include "connection.c"
#include "error.c"

#define MAXARGS 9
#define MINARGS 1

#define MAX(a, b) a*(a>b) + b*(b>=a)

time_t t;

int fd;
fd_set rset;
struct addrinfo hints, *res;
socklen_t addrlen;
struct sockaddr_in addr;

char fsip[32], fsport[8], asip[32], asport[8];

void tcpConnect() {
    int n;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) errorExit("socket()");

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET; // IPv4
    hints.ai_socktype=SOCK_STREAM; // TCP socket

    n = getaddrinfo(asip, asport, &hints, &res);
    if (n != 0) errorExit("getaddrinfo()");

    n = connect(fd, res->ai_addr, res->ai_addrlen);
    if (n == -1) errorExit("connect()");

}

void login(char* uid, char* pass) {
    int n;
    char message[64];
    sprintf(message, "LOG %s %s\n", uid, pass);
    printf("our message: %s", message);
    n = write(fd, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void requestFile(char *uid, char* rid) {
    int n;
    char message[64], file[32];
    char op;

    scanf("%c", &op);
    switch(op) {
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
            printf("%c %s\n", op, file);
            printf("%s %s %c %s\n", uid, rid, op, file);
            sprintf(message, "REQ %s %s %c %s\n", uid, rid, op, file);
            break;
        case 'X':
            sprintf(message, "REQ %s %s %c\n", uid, rid, op);
            break;
    }
    printf("our message: %s\n", message);
    n = write(fd, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void validateCode(char* uid, char*rid, char* vc) {
    int n;
    char message[64];

    sprintf(message, "AUT %s %s %s\n \n", uid, rid, vc);
    printf("our message: %s", message);
    n = write(fd, message, strlen(message));
    if (n == -1) errorExit("write()");
}

int main(int argc, char* argv[]) {
    char command[6], uid[7], pass[10], reply[9], vc[4], rid[5], tid[5], acr[4];
    char op;

    int n, i, maxfdp1;

    //srand((unsigned) time(&t));
    
    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s -n ASIP] [-p ASport] [-m FSIP] [-q FSport]\n", argv[0]);
        errorExit("incorrect number of arguments");
    }

    strcpy(asip, ASIP);
    strcpy(asport, ASPORT);
    strcpy(fsip, FSIP);
    strcpy(fsport, FSPORT);

    printf("%s\n", asip);
    printf("%s\n", asport);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-n")) {
            strcpy(asip, argv[++i]);
            printf("%s\n", asip);
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
            printf("%s\n", asport);
        } else if (!strcmp(argv[i], "-m")) {
            strcpy(fsip, argv[++i]);  
        } else if (!strcmp(argv[i], "-q")) {
            strcpy(fsport, argv[++i]);
        }
    }

    tcpConnect(); 

    strcpy(rid, "1234");

    while(1) {
        //printf("Inside select\n");
        FD_ZERO(&rset); 
        //printf("Inside select 1\n");
        FD_SET(STDIN, &rset);
        //printf("Inside select 2\n");
        FD_SET(fd, &rset);
        //printf("Inside select 3\n");


        maxfdp1 = MAX(STDIN, fd) + 1;
        //printf("Inside select 4\n");

        n = select(maxfdp1, &rset, NULL, NULL, NULL);
        if (n == -1) errorExit("select()");

        //printf("Inside select 5\n");


        if (FD_ISSET(fd, &rset)) {
            //printf("hey\n");
            n = read(fd, reply, 9);
            if (n == -1) errorExit("read()");
            reply[n] = '\0';

            //printf("server reply: %s", reply); /* debug */ // TODO remove this

            sscanf(reply, "%s %s", acr, tid);

            if (!strcmp(reply, "RLO OK\n"))
                printf("You are now logged in.\n");
            else if (!strcmp(reply, "RLO NOK\n"))
                printf("Login was a failureeee you a failureeee\n"); // TODO change this
            else if (!strcmp(reply, "RRQ OK\n"))
                printf("Request successful\n"); // TODO change this
            else if (!strcmp(reply, "RRQ NOK\n"))
                printf("Request was a failureeee you a failureeee\n"); // TODO change this
            else if (!strcmp(acr, "AUT"))
                printf("Authenticated! (TID=%s)", tid);  /* debug */ // TODO remove this
            //else
                //printf("nope, not working\n");
            
        }
        
        if (FD_ISSET(STDIN, &rset)) {
                scanf("%s", command);
                printf("%s\n", command);
                if (!strcmp(command, "login")) {
                    scanf("%s %s", uid, pass);
                    printf("%s %s\n", uid, pass);
                    login(uid, pass);
                    printf("after login duh\n");
                } else if (!strcmp(command, "req")) {
                    scanf("%c", &op);
                    requestFile(uid, rid);
                }
                else if (!strcmp(command, "val")) {
                    scanf("%s %s %s", uid, rid, vc);
                    validateCode(uid, rid, vc);
                }
                else printf("ERR\n");    /* debug */ // TODO remove this
        }
    }

    return 0;
}
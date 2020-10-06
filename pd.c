#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include "config.h"

#define MAXARGS 8
#define MINARGS 2

#define MAX(a, b) a*(a>b) + b*(b>a)

int fd_udp_client, fd_udp;
fd_set rset;
struct addrinfo hints_udp_client, *res_udp_client, hints_udp, *res_udp;
struct sockaddr_in addr_udp;
socklen_t addrlen_udp;
struct sockaddr_in addr_udp;

char pdip[32], pdport[8], asip[32], asport[8];

void errorExit(char* errorMessage) {
    printf("ERROR: %s\n", errorMessage);
    exit(1);
}

void udpOpenConnection() {
    int errcode;
    ssize_t n;
    fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1) errorExit("socket()");
    memset(&hints_udp, 0, sizeof hints_udp);
    hints_udp.ai_family = AF_INET;          //IPv4
    hints_udp.ai_socktype = SOCK_DGRAM;     //UDP socket
    hints_udp.ai_flags=AI_PASSIVE;

    errcode = getaddrinfo(NULL, pdport, &hints_udp, &res_udp);
    if (errcode != 0) errorExit("getaddrinfo()");

    n = bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1) errorExit("bind()");
}

void udpConnect() {
    int n;

    fd_udp_client = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp_client == -1) errorExit("socket()");

    memset(&hints_udp_client, 0, sizeof hints_udp_client);
    hints_udp_client.ai_family=AF_INET; // IPv4
    hints_udp_client.ai_socktype=SOCK_DGRAM; // UDP socket

    n = getaddrinfo(asip, asport, &hints_udp_client, &res_udp_client);
    if (n != 0) errorExit("getaddrinfo()");
}

void registration(char* uid, char* pass) {
    int n;
    char message[64];

    if (strlen(uid) != 5 || strlen(pass) != 8) {
        printf("ERR2\n");   /* debug */ // TODO change this
        return;
    }

    sprintf(message, "REG %s %s %s %s\n", uid, pass, pdip, pdport);
    printf("our message: %s", message);
    n = sendto(fd_udp_client, message, strlen(message)*sizeof(char), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) errorExit("sendto()");
}

char* validateRequest(char* message) {
    char command[5], uid[32], vc[32], fname[32], reply[128], fop;
    printf("message from AS: %s", message);
    sscanf(message, "%s %s %s %s", command, uid, vc, fop);
    if (!strcmp(command, "VLC") && strlen(vc) == 4) {
        if (fop == 'R' || fop == 'U' || fop == 'D')
            scanf("%s", fname);
        printf("%s", message);
    } else {
        return "ERR";
    }
}

void unregistration() {
    freeaddrinfo(res_udp_client);
    close(fd_udp_client);
    exit(0);
}

int main(int argc, char* argv[]) {
    char command[6], uid[7], pass[10], reply[9], buffer[32];
    int n, i, maxfdp1;
    
    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s PDIP [-d PDport] [-n ASIP] [-p ASport]\n", argv[0]);
        errorExit("incorrect number of arguments");
    }

    strcpy(pdip, argv[1]);
    strcpy(pdport, PDPORT);
    strcpy(asip, ASIP);
    strcpy(asport, ASPORT);

    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-d")) {
            strcpy(pdport, argv[++i]);  
        } else if (!strcmp(argv[i], "-n")) {
            strcpy(asip, argv[++i]);
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        }
    }

    udpOpenConnection();
    udpConnect();

    FD_ZERO(&rset); 
    maxfdp1 = MAX(STDIN, fd_udp_client) + 1;

    while(1) {
        FD_SET(STDIN, &rset);
        FD_SET(fd_udp_client, &rset);
        FD_SET(fd_udp, &rset);

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd_udp_client, &rset)) {
            n = recvfrom(fd_udp_client, reply, 9, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
            if (n == -1) errorExit("recvfrom()");

            printf("server reply: %s", reply); /* debug */ // TODO remove this

            if (!strcmp(reply, "RRG OK\n"))
                printf("Registration successful\n");
            else if (!strcmp(reply, "RRG NOK\n"))
                printf("Registration was a failureeee you a failureeee\n"); // TODO change this
            else
                printf("nope, not working\n");  /* debug */ // TODO remove this
        }

        if (FD_ISSET(fd_udp, &rset)) {
            n = recvfrom(fd_udp, buffer, 32, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
            if (n == -1) errorExit("recvfrom()");
            printf("received message from as\n");

            n = sendto(fd_udp, validateRequest(buffer), n, 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if (n == -1) errorExit("sendto()");
        }
        
        if (FD_ISSET(STDIN, &rset)) {
            scanf("%s", command);
            if (!strcmp(command, "reg")) {
                scanf("%s %s", uid, pass);
                registration(uid, pass);
            } else if (!strcmp(command, "exit")) {
                unregistration();
            } else printf("ERR1\n");    /* debug */ // TODO remove this
        }
    }

    return 0;
}
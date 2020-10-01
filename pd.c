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
#define STDIN 0

#define MAX(a, b) a*(a>b) + b*(b>a)

int fd;
fd_set rset;
struct addrinfo hints, *res;
socklen_t addrlen;
struct sockaddr_in addr;

char pdip[32], pdport[8], asip[32], asport[8];

void errorExit(char* errorMessage) {
    printf("ERROR: %s\n", errorMessage);
    exit(1);
}

void udpConnect() {
    int n;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) errorExit("socket()");

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET; // IPv4
    hints.ai_socktype=SOCK_DGRAM; // UDP socket

    n = getaddrinfo(asip, asport, &hints, &res);
    if (n != 0) errorExit("getaddrinfo()");
}

void registration(char* uid, char* pass) {
    int n;
    char message[64];

    if (strlen(uid) != 5 || strlen(pass) != 8) {
        printf("ERR2\n");   /* debug */
        return;
    }

    sprintf(message, "REG %s %s %s %s\n", uid, pass, pdip, pdport);
    printf("our message: %s", message);
    n = sendto(fd, message, strlen(message)*sizeof(char), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) errorExit("sendto()");
}

void unregistration() {
    freeaddrinfo(res);
    close(fd);
    exit(0);
}

int main(int argc, char* argv[]) {
    char command[6], uid[7], pass[10], reply[9], buffer[32];
    int n, i, maxfdp1;
    
    if (argc < 2 || argc > 8) {
        printf("​Usage: %s PDIP [-d PDport] [-n ASIP] [-p ASport]\n", argv[0]);
        errorExit("incorrect number of arguments");
    }

    strcpy(pdip, argv[1]);
    strcpy(pdport, "5000");
    strcpy(asip, "tejo.tecnico.ulisboa.pt");
    strcpy(asport, "58011");

    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-d")) {
            strcpy(pdport, argv[++i]);  
        } else if (!strcmp(argv[i], "-n")) {
            strcpy(asip, argv[++i]);
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        }
    }

    udpConnect();

    FD_ZERO(&rset); 
    maxfdp1 = MAX(STDIN, fd) + 1;

    while(1) {
        FD_SET(STDIN, &rset);
        FD_SET(fd, &rset);

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd, &rset)) {
            n = recvfrom(fd, reply, 9, 0, (struct sockaddr*) &addr, &addrlen);
            if (n == -1) errorExit("recvfrom()");

            printf("server reply: %s", reply); /* debug */

            if (!strcmp(reply, "RRG OK\n"))
                printf("Registration successful\n");
            else if (!strcmp(reply, "RRG NOK\n"))
                printf("Registration was a failureeee you a failureeee\n");
            else
                printf("nope, not working\n");  /* debug */
        }
        
    if (FD_ISSET(STDIN, &rset)) {
            scanf("%s", command);
            if (!strcmp(command, "reg")) {
                scanf("%s %s", uid, pass);
                registration(uid, pass);
            } else if (!strcmp(command, "exit")) {
                unregistration();
            } else printf("ERR1\n");    /* debug */
        }
    }

    return 0;
}
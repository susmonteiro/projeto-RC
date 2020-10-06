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

#define MAXARGS 4
#define MINARGS 1

#define MAX(a, b) a*(a>b) + b*(b>a)

int numClients = 0;

int fd_udp, fd_udp_client, fd_tcp;
int* fd_array;
fd_set rset;
char verbose = FALSE;
socklen_t addrlen_udp, addrlen_udp_client, addrlen_tcp;
struct addrinfo hints_udp, *res_udp, hints_udp_client, *res_udp_client, hints_tcp, *res_tcp;
struct sockaddr_in addr_udp, addr_udp_client, addr_tcp;

char asport[8], pdip[32], pdport[8];

void errorExit(char* errorMessage) {
    printf("ERROR: %s\n", errorMessage);
    exit(1);
}

void udpOpenConnection() {
    int errcode;
    ssize_t n;
    fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1) errorExit("UDP server: socket()");
    memset(&hints_udp, 0, sizeof hints_udp);
    hints_udp.ai_family = AF_INET;          //IPv4
    hints_udp.ai_socktype = SOCK_DGRAM;     //UDP socket
    hints_udp.ai_flags=AI_PASSIVE;

    errcode = getaddrinfo(NULL, asport, &hints_udp, &res_udp);
    if (errcode != 0) errorExit("UDP server: getaddrinfo()");

    n = bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1) errorExit("UDP server: bind()");
}

void udpConnect() {
    int n;

    fd_udp_client = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp_client == -1) errorExit("UDP client: socket()");

    memset(&hints_udp_client, 0, sizeof hints_udp_client);
    hints_udp_client.ai_family=AF_INET; // IPv4
    hints_udp_client.ai_socktype=SOCK_DGRAM; // UDP socket

    n = getaddrinfo(pdip, pdport, &hints_udp_client, &res_udp_client);
    if (n != 0) errorExit("UDP client: getaddrinfo()");
}

char* login(char* uid, char* pass) {
    printf("unimplemented\n");
    return "RLO OK\n";
}

char* request(char* uid, char* rid, char* fop, char* fname) {
    char message[32], reply[32];
    int n;
    char *vc = "1234";  // TODO

    printf("unimplemented\n");
    sprintf(message, "VLC %s %s %s %s\n", uid, vc, fop, fname);

    n = sendto(fd_udp_client, message, strlen(message)*sizeof(char), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) errorExit("request: sendto()");
    n = recvfrom(fd_udp_client, reply, 32, 0, (struct sockaddr*) &addr_udp_client, &addrlen_udp_client);
    if (n == -1) errorExit("request: recvfrom()");

    if (!strcmp(reply, "RVC OK\n")) {
        return "RRQ OK\n";
    } else if (!strcmp(reply, "RVC NOK\n")) {
        return "RRQ NOK\n";
    } else {
        return "ERR\n";
    }
}

char* secondAuthentication(char* uid, char* rid, char* vc) {
    printf("unimplemented\n");
    return "RAU TID";   // TODO tid
}

void userSession(int fd) {
    int n;
    char buffer[128];
    char command[5], arg1[32], arg2[32], arg3[32], arg4[32];

    n = read(fd, buffer, 128 * sizeof(char));
    if (n == -1) errorExit("userSession: read()");
    sscanf(buffer, "%s %s %s %s %s", command, arg1, arg2, arg3, arg4);

    if (!strcmp(command, "LOG")) {
        strcpy(buffer, login(arg1, arg2));
    } else if (!strcmp(command, "REQ")) {
        strcpy(buffer, request(arg1, arg2, arg3, arg4));
    } else if (!strcmp(command, "AUT")) {
        strcpy(buffer, secondAuthentication(arg1, arg2, arg3));
    }
    n = write(fd, buffer, strlen(buffer)*sizeof(char));
    if (n == -1) errorExit("userSession: write()");
}

void tcpOpenConnection() {
    int errcode;

    ssize_t n;
    char buffer[128];

    fd_tcp = socket(AF_INET,SOCK_STREAM,0);
    if(fd_tcp == -1) errorExit("TCP: socket()");

    memset(&hints_tcp, 0, sizeof hints_tcp);
    hints_tcp.ai_family = AF_INET;
    hints_tcp.ai_socktype = SOCK_STREAM;
    hints_tcp.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(NULL, ASPORT, &hints_tcp, &res_tcp);
    if(errcode != 0) errorExit("TCP: getaddrinfo()");

    n = bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen);
    if(n == -1) errorExit("TCP: bind()");
    
    if(listen(fd_tcp, 5) == -1) errorExit("TCP: listen()");
    fd_array = (int*)malloc(sizeof(int*));

    /* freeaddrinfo(res_tcp); // TODO move this
    close(fd_tcp); */
}


char* registration(char* uid, char* pass, char* pdip_new, char* pdport_new) {
    printf("unimplemented\n");
    printf("PD: new user, UID = %s\n", uid);
    strcpy(pdip, pdip_new);
    strcpy(pdport, pdport_new);
    udpConnect();
    return "RRG OK\n";
}

char* unregistration(char* uid, char* pass) {
    printf("unimplemented\n");
    return "RUN OK\n";
}

char* applyCommand(char* message) {
    char command[5], arg1[32], arg2[32], arg3[32], arg4[32], reply[128];
    printf("message from PD: %s", message);
    sscanf(message, "%s %s %s %s %s", command, arg1, arg2, arg3, arg4);
    if (!strcmp(command, "REG")) {
        return registration(arg1, arg2, arg3, arg4);
    } else if (!strcmp(command, "UNR")) {
        return unregistration(arg1, arg2);
    } else {
        return "ERR";
    }
}

int main(int argc, char* argv[]) {
    char buffer[128];
    int i, n, maxfdp1;
    
    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s -p [ASport] [-v]\n", argv[0]);
        errorExit("incorrect number of arguments");
    }

    strcpy(asport, ASPORT);

    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            verbose = TRUE;  
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        }
    }

    udpOpenConnection();
    tcpOpenConnection();

    printf("i connected yey\n");

    FD_ZERO(&rset); 
    maxfdp1 = MAX(fd_tcp, fd_udp) + 1;

    while(1) {
        FD_SET(fd_udp, &rset);
        FD_SET(fd_tcp, &rset);

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd_udp, &rset)) {
            n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
            if (n == -1) errorExit("main: recvfrom()");
            printf("received message from pd\n");
            
            n = sendto(fd_udp, applyCommand(buffer), 9, 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if (n == -1) errorExit("main: sendto()");
        } if (FD_ISSET(fd_tcp, &rset)) {
            if((fd_array[numClients++] = accept(fd_tcp, (struct sockaddr *) &addr_tcp, &addrlen_tcp)) == -1) errorExit("main: accept()");

            fd_array = (int*)realloc(fd_array, numClients + 1);
            userSession(fd_array[numClients - 1]);

            for (i=0; i<numClients; i++) {
                maxfdp1 = MAX(maxfdp1, fd_array[i]);
            }
        }

        for (i=0; i<numClients; i++) {
            if (FD_ISSET(fd_array[i], &rset)) {
                userSession(fd_array[i]);
            }
        }
    }

    freeaddrinfo(res_udp);
    close(fd_udp);

    return 0;
}
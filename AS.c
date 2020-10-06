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
#include <pthread.h>
#include "config.h"

#define MAXARGS 4
#define MINARGS 1

#define MAX(a, b) a*(a>b) + b*(b>a)

int numClients = 0;
pthread_t *tcp_threads;

int fd_udp, fd_udp_client;
fd_set rset;
char verbose = FALSE;
socklen_t addrlen_udp, addrlen_udp_client;
struct addrinfo hints_udp, *res_udp, hints_udp_client, *res_udp_client;
struct sockaddr_in addr_udp, addr_udp_client;

char asport[8], pdip[32], pdport[8];

void errorExit(char* errorMessage) {
    printf("ERROR: %s\n", errorMessage);
    exit(1);
}

void udpOpenConnection() {
    int errcode;
    ssize_t n;
    fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp == -1) exit(1);
    memset(&hints_udp, 0, sizeof hints_udp);
    hints_udp.ai_family = AF_INET;          //IPv4
    hints_udp.ai_socktype = SOCK_DGRAM;     //UDP socket
    hints_udp.ai_flags=AI_PASSIVE;

    errcode = getaddrinfo(NULL, asport, &hints_udp, &res_udp);
    if (errcode != 0) exit(1);

    n = bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1) exit(1);
}

void udpConnect() {
    int n;

    fd_udp_client = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_udp_client == -1) errorExit("socket()");

    memset(&hints_udp_client, 0, sizeof hints_udp_client);
    hints_udp_client.ai_family=AF_INET; // IPv4
    hints_udp_client.ai_socktype=SOCK_DGRAM; // UDP socket

    n = getaddrinfo(pdip, pdport, &hints_udp_client, &res_udp_client);
    if (n != 0) errorExit("getaddrinfo()");
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
    if (n == -1) errorExit("sendto()");
    n = recvfrom(fd_udp_client, reply, 32, 0, (struct sockaddr*) &addr_udp_client, &addrlen_udp_client);
    if (n == -1) errorExit("recvfrom()");

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

void *userSession(void* fd_tcp) {
    int n;
    char buffer[128];
    char command[5], arg1[32], arg2[32], arg3[32], arg4[32];
    
    int fd = *((int*) fd_tcp);

    read(fd, buffer, 128 * sizeof(char));
    sscanf(buffer, "%s %s %s %s %s", command, arg1, arg2, arg3, arg4);

    if (!strcmp(command, "LOG")) {
        strcpy(buffer, login(arg1, arg2));
    } else if (!strcmp(command, "REQ")) {
        strcpy(buffer, request(arg1, arg2, arg3, arg4));
    } else if (!strcmp(command, "AUT")) {
        strcpy(buffer, secondAuthentication(arg1, arg2, arg3));
    }
    n = write(fd, buffer, strlen(buffer)*sizeof(char));
    if (n == -1) errorExit("write()");
}

void tcpOpenConnection() {
    int fd_tcp, newfd, errcode;

    ssize_t n;
    socklen_t addrlen_tcp;
    struct addrinfo hints_tcp, *res_tcp;
    struct sockaddr_in addr_tcp;
    char buffer[128];

    fd_tcp = socket(AF_INET,SOCK_STREAM,0);
    if(fd_tcp == -1) exit(1);

    memset(&hints_tcp, 0, sizeof hints_tcp);
    hints_tcp.ai_family = AF_INET;
    hints_tcp.ai_socktype = SOCK_STREAM;
    hints_tcp.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(NULL, ASPORT, &hints_tcp, &res_tcp);
    if(errcode != 0) exit(1);

    n = bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen);
    if(n == -1) exit(1);
    
    if(listen(fd_tcp, 5) == -1) exit(1);
    tcp_threads = (pthread_t*)malloc(sizeof(pthread_t*));

    while(1) {
        if((newfd = accept(fd_tcp, (struct sockaddr *) &addr_tcp, &addrlen_tcp)) == -1) errorExit("accept()");

        pthread_create(&tcp_threads[numClients++], NULL, userSession, (void*) &newfd);
        tcp_threads = (pthread_t*)realloc(tcp_threads, numClients);
    }

    freeaddrinfo(res_tcp);
    close(fd_tcp);
}


char* registration(char* uid, char* pass, char* pdip_new, char* pdport_new) {
    printf("unimplemented\n");
    printf("PD: new user, UID = %s", uid);
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
    char buffer[34];
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
    tcpOpenConnection(); // TODO move to select

    printf("i connected yey");

    FD_ZERO(&rset); 
    maxfdp1 = MAX(STDIN, fd_udp) + 1;

    while(1) {
        FD_SET(STDIN, &rset);
        FD_SET(fd_udp, &rset);

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd_udp, &rset)) {
            n = recvfrom(fd_udp, buffer, 34, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
            if (n == -1) errorExit("recvfrom()");
            printf("received message from pd\n");

            n = sendto(fd_udp, applyCommand(buffer), n, 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if (n == -1) errorExit("sendto()");
        }
    }

    freeaddrinfo(res_udp);
    close(fd_udp);

    return 0;
}
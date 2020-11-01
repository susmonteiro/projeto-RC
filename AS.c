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
#include "connection.h"
#include "error.h"

#define MAXARGS 4
#define MINARGS 1

#define MAX(a, b) a*(a>b) + b*(b>=a)

int numClients = 0;
int verbose = FALSE;

int fd_udp, fd_udp_client, fd_tcp;
int* fd_array;
fd_set rset;
socklen_t addrlen_udp, addrlen_udp_client, addrlen_tcp;
struct addrinfo hints_udp, *res_udp, hints_udp_client, *res_udp_client, hints_tcp, *res_tcp;
struct sockaddr_in addr_udp, addr_udp_client, addr_tcp;

char asport[8], pdip[32], pdport[8];

// print if verbose mode
void printv(char* message) {
    if (verbose == TRUE) printf("%s\n", message);
}

char* login(char* uid, char* pass) {
    char message[64];
    printv("unimplemented");
    sprintf(message, "User: login ok, UID=%s", uid);
    printv(message);
    return "RLO OK\n";
}

 int random_int(int min, int max) {
    return min + rand() % (max+1 - min);
}

char* request(char* uid, char* rid, char* fop, char* fname) {
    char message[64], reply[32];
    int n, vc;

    printf("%c", fname[0]);

    printf("booh\n");
    
    vc = rand() % 9999;
    
    if (fname[0] != '\0') {
        printf("entrouuuuu\n");
        sprintf(message, "VLC %s %d %s %s\n", uid, vc, fop, fname);
        printf("message: %s, continua\n", message);
    }
    else sprintf(message, "VLC %s %d %s\n", uid, vc, fop);

    n = sendto(fd_udp_client, message, strlen(message), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) printError("request: sendto()");
    printf("%d\n", n);
    n = recvfrom(fd_udp_client, reply, 32, 0, (struct sockaddr*) &addr_udp_client, &addrlen_udp_client);
    if (n == -1) printError("request: recvfrom()");
    reply[n] = '\0';

    if (!strcmp(reply, "RVC OK\n")) {
        if (fname[0] == '\0') {
            sprintf(message, "User: upload req, UID=%s, RID=%s VC=%d", uid, rid, vc);
            printv(message);
        } else {
            sprintf(message, "User: upload req, UID=%s file:%s, RID=%s VC=%d\n", uid, fname, rid, vc);
            printv(message);
        }
        return "RRQ OK\n";
    } else if (!strcmp(reply, "RVC NOK\n")) {
        return "RRQ NOK\n";
    } else {
        return "ERR\n";
    }
}

char* secondAuthentication(char* uid, char* rid, char* vc) {
    int tid;
    char message[64];
    char *res;
    tid = rand() % 9999;

    printv("unimplemented\n");
    sprintf(message, "User: UID=%s", uid);
    printv(message);
    sprintf(message, "U, f1.txt, TID=%d", tid);  //possivelmente vamos ter que criar um vetor de estruturas que guardam file requests para guardar os opcodes e assim (com id rid por ex.)
    printv(message);
    res = (char*)malloc(64*sizeof(char));
    sprintf(res, "RAU %d\n", tid);
    return res;   // TODO tid
}

void userSession(int fd) {
    int n;
    char buffer[128], command[5], arg1[32], arg2[32], arg3[32], arg4[32];

    n = read(fd, buffer, 128);
    if (n == -1) printError("userSession: read()");
    buffer[n] = '\0';

    printf("%s", buffer);

    sscanf(buffer, "%s %s %s %s", command, arg1, arg2, arg3);

    if (!strcmp(arg3, "D") || !strcmp(arg3, "R") || !strcmp(arg3, "U")) {
        printf("entrei1\n");
        sscanf(buffer, "%s %s %s %s %s", command, arg1, arg2, arg3, arg4);
        printf("entrei2\n");
    } else {
        arg4[0] = '\0';
    }
    if (!strcmp(command, "LOG")) {
        strcpy(buffer, login(arg1, arg2));

    } else if (!strcmp(command, "REQ")) {
        printf("entrei3\n");
        strcpy(buffer, request(arg1, arg2, arg3, arg4));
    } else if (!strcmp(command, "AUT")) {
        strcpy(buffer, secondAuthentication(arg1, arg2, arg3));
        printf("%s\n", buffer);
    }
    n = write(fd, buffer, strlen(buffer));
    if (n == -1) printError("userSession: write()"); 
}



char* registration(char* uid, char* pass, char* pdip_new, char* pdport_new) {
    char message[64];
    printv("unimplemented");
    sprintf(message, "PD: new user, UID = %s", uid);
    printv(message);
    strcpy(pdip, pdip_new);
    strcpy(pdport, pdport_new);
    udpConnect(pdip, pdport, &fd_udp_client, &res_udp_client);
    return "RRG OK\n";
}

char* unregistration(char* uid, char* pass) {
    printv("unimplemented");
    return "RUN OK\n";
}

char* applyCommand(char* message) {
    char command[5], arg1[32], arg2[32], arg3[32], arg4[32];
    char msg[64];
    sprintf(msg, "message from PD: %s", message);
    printv(msg);
    sscanf(message, "%s %s %s %s %s", command, arg1, arg2, arg3, arg4);
    if (!strcmp(command, "REG")) {
        return registration(arg1, arg2, arg3, arg4);
    } else if (!strcmp(command, "UNR")) {
        return unregistration(arg1, arg2);
    } else {
        return "ERR\n";
    }
}

void endAS() {
    int i = 0;

    freeaddrinfo(res_udp);
    freeaddrinfo(res_udp_client);
    close(fd_udp);
    close(fd_udp_client);

    for (i=0; i< numClients; i++) close(fd_array[i]);
    freeaddrinfo(res_tcp);
    close(fd_tcp);
    printf("goodbye\n");
    exit(0);
}

int main(int argc, char* argv[]) {
    char buffer[128];
    int i, n, maxfdp1;
    
    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s -p [ASport] [-v]\n", argv[0]);
        printError("incorrect number of arguments");
    }

    strcpy(asport, ASPORT);

    for (i = MINARGS; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            verbose = TRUE;  
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        }
    }

    tcpOpenConnection(asport, &fd_tcp, &res_tcp);
    if (listen(fd_tcp, 5) == -1) printError("TCP: listen()");
    fd_array = (int*)malloc(sizeof(int*));

    udpOpenConnection(asport, &fd_udp, &res_udp);
    addrlen_udp = sizeof(addr_udp);

    signal(SIGINT, endAS);

    printv("i connected yey");

    while(1) {
        FD_ZERO(&rset);
        FD_SET(fd_udp, &rset);
        FD_SET(fd_tcp, &rset);

        for (i=0; i<numClients; i++)
            FD_SET(fd_array[i], &rset);

        maxfdp1 = MAX(fd_tcp, fd_udp) + 1;
        
        for (i=0; i<numClients; i++) {
                maxfdp1 = MAX(maxfdp1, fd_array[i]) + 1;
        } 

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd_udp, &rset)) {
            n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
            if (n == -1) printError("main: recvfrom()");
            buffer[n] = '\0';
            printv("received message from pd");

            n = sendto(fd_udp, applyCommand(buffer), 32, 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if (n == -1) printError("main: sendto()");
        } if (FD_ISSET(fd_tcp, &rset)) {
            printv("entrei");
            if((fd_array[numClients++] = accept(fd_tcp, (struct sockaddr *) &addr_tcp, &addrlen_tcp)) == -1) printError("main: accept()");

            fd_array = (int*)realloc(fd_array, numClients + 1);
            //userSession(fd_array[numClients - 1]);
        }

        for (i=0; i<numClients; i++) {
            if (FD_ISSET(fd_array[i], &rset)) {
                userSession(fd_array[i]);
            }
        }
    }
    
    //closeConnection(fd_udp, res_udp);

    return 0;
}

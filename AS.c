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
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include "config.h"
#include "connection.h"
#include "error.h"

#define MAXARGS 4
#define MINARGS 1

#define MAX(a, b) a*(a>b) + b*(b>=a)

typedef struct tcp_client {
    int fd;
    char uid[7];
} tcp_client;

int numClients = 0;
int verbose = FALSE;

int fd_udp, fd_udp_client, fd_tcp;
tcp_client* fd_array;
fd_set rset;
socklen_t addrlen_udp, addrlen_udp_client, addrlen_tcp;
struct addrinfo hints_udp, *res_udp, hints_udp_client, *res_udp_client, hints_tcp, *res_tcp;
struct sockaddr_in addr_udp, addr_udp_client, addr_tcp;

char asport[8], pdip[32], pdport[8];

// print if verbose mode
void printv(char* message) {
    if (verbose) printf("%s\n", message);
}

char* login(int ind, char* uid, char* pass) {
    char message[64], path[32], currentPass[32];
    FILE *file;

    sprintf(path, "registrations/%s", uid);
    if ((file = fopen(path, "r")) == NULL)
        return "RLO NOK\n";
        
    fscanf(file, "%s", currentPass);
    fclose(file);
    if (strcmp(currentPass, pass))
        return "RLO NOK\n";
    
    printv(uid);
    strcpy(fd_array[ind].uid, uid);
    fd_array[ind].uid[5] = '\0';
    printv(uid);
    sprintf(message, "User: login ok, UID=%s", uid);
    printv(message);
    return "RLO OK\n";
}

void request(char* uid, char* rid, char* fop, char* fname) {
    char message[64], reply[32];
    int n, vc;

    printf("booh\n"); // DEBUG

    vc = rand() % 9999;

    if (fname[0] == '\0') {
        sprintf(message, "User: %s req, UID=%s, RID=%s VC=%d", fop, uid, rid, vc);
        printv(message);
    } else {
        sprintf(message, "User: %s req, UID=%s file:%s, RID=%s VC=%d", fop, uid, fname, rid, vc);
        printv(message);
    }
    
    if (fname[0] != '\0') {
        printf("entrouuuuu\n"); // DEBUG
        sprintf(reply, "VLC %s %d %s %s\n", uid, vc, fop, fname);
        printf("reply: %s, continua\n", reply);
    }
    else sprintf(reply, "VLC %s %d %s\n", uid, vc, fop);

    n = sendto(fd_udp_client, reply, strlen(reply), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) printError("request: sendto()");
}

void confirm_validation(char* reply) {
    char uid[7], state[5], message[32];
    int i, n;
    printf("olaaaa\n"); // DEBUG
    printf("%s", reply);
    sscanf(reply, "RVC %s %s", uid, state);

    for(i = 0; i < numClients; i++) {
        printf("%s", fd_array[i].uid);
        if (!strcmp(fd_array[i].uid, uid)) {
            printf("yeeeey\n"); // DEBUG
            if (!strcmp(state, "OK"))
                strcpy(message, "RRQ OK\n");
            else if (!strcmp(state, "NOK"))
                strcpy(message, "RRQ NOK\n");
            else
                strcpy(message, "ERR\n");
            n = write(fd_array[i].fd, message, strlen(message));
            if (n == -1) printError("userSession: write()");
            break;
        }
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
    res = (char*)malloc(32*sizeof(char));
    sprintf(res, "RAU %d\n", tid);
    return res;   // TODO tid
}

void userSession(int ind) {
    int n, fd;
    char buffer[128], msg[256], command[5], arg1[32], arg2[32], arg3[32], arg4[32];

    n = read(fd_array[ind].fd, buffer, 128);
    if (n == -1) printError("userSession: read()");
    else if (n == 0) {
        fd = fd_array[ind].fd;
        printf("vai fechar\n"); // DEBUG
        close(fd);
        fd_array[ind].fd = -5;
        return;
    }

    buffer[n] = '\0';
    sprintf(msg, "message from User: %s", buffer);
    printv(msg);
    sscanf(buffer, "%s %s %s %s", command, arg1, arg2, arg3);

    if (!strcmp(arg3, "D") || !strcmp(arg3, "R") || !strcmp(arg3, "U")) {
        sscanf(buffer, "%s %s %s %s %s", command, arg1, arg2, arg3, arg4);
    } else {
        arg4[0] = '\0';
    }
    if (!strcmp(command, "LOG")) {
        strcpy(buffer, login(ind, arg1, arg2));

    } else if (!strcmp(command, "REQ")) {
        request(arg1, arg2, arg3, arg4);
    } else if (!strcmp(command, "AUT")) {
        strcpy(buffer, secondAuthentication(arg1, arg2, arg3));
        printf("%s\n", buffer);
    }
    if (strcmp(command, "REQ")) {
        printf("whats up\n"); // DEBUG
        n = write(fd_array[ind].fd, buffer, strlen(buffer));
        if (n == -1) printError("userSession: write()"); 
    }
}

char* registration(char* uid, char* pass, char* pdip_new, char* pdport_new) {
    char message[64], path[32];
    FILE *file;

    sprintf(path, "registrations/%s", uid);
    if (access(path, F_OK) != -1)
        return "RRG NOK\n";

    file = fopen(path, "w");
    fprintf(file, "%s\n%s\n%s", pass, pdip_new, pdport_new);
    fclose(file);
    sprintf(message, "PD: new user, UID = %s", uid);
    printv(message);
    /* strcpy(pdip, pdip_new);
    strcpy(pdport, pdport_new);
    udpConnect(pdip, pdport, &fd_udp_client, &res_udp_client); */ // TODO remove this
    return "RRG OK\n";
}

char* unregistration(char* uid, char* pass) {
    char path[32], currentPass[32];
    FILE *file;

    sprintf(path, "registrations/%s", uid);
    if ((file = fopen(path, "r")) == NULL)
        return "RUN NOK\n";
        
    fscanf(file, "%s", currentPass);
    fclose(file);
    if (strcmp(currentPass, pass))
        return "RUN NOK\n";

    remove(path);
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
    freeaddrinfo(res_tcp);
    close(fd_tcp);
    for (i=0; i<numClients; i++)
        close(fd_array[i].fd);
    printf("goodbye\n"); // DEBUG
    exit(0);
}

int main(int argc, char* argv[]) {
    char buffer[128], reply[128];
    int i, n, maxfdp1, tcp_flag = 0;
    
    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s -p [ASport] [-v]\n", argv[0]);
        printError("incorrect number of arguments");
    }

    strcpy(asport, ASPORT);

    for (i = MINARGS; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            printf("​Usage: %s -p [ASport] [-v]\n", argv[0]);
            exit(0);
        } else if (!strcmp(argv[i], "-v")) {
            verbose = TRUE;  
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        }
    }

    mkdir("registrations", 0777);

    tcpOpenConnection(asport, &fd_tcp, &res_tcp);
    if (listen(fd_tcp, 5) == -1) printError("TCP: listen()");
    fd_array = (tcp_client*)malloc(sizeof(struct tcp_client));

    udpOpenConnection(asport, &fd_udp, &res_udp);
    addrlen_udp = sizeof(addr_udp);

    signal(SIGINT, endAS);

    printv("i connected yey");

    while(1) {
        printf("continuo aqui\n"); // DEBUG
        FD_ZERO(&rset);
        FD_SET(fd_udp, &rset);
        FD_SET(fd_udp_client, &rset);
        FD_SET(fd_tcp, &rset);

        for (i=0; i<numClients; i++) {
            if (fd_array[i].fd != -5)
                FD_SET(fd_array[i].fd, &rset);
        }

        maxfdp1 = MAX(MAX(fd_tcp, fd_udp), fd_udp_client) + 1;
        
        for (i=0; i<numClients; i++) {
            maxfdp1 = MAX(maxfdp1, fd_array[i].fd) + 1;
        } 

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd_udp, &rset)) {
            n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
            if (n == -1) printError("main: recvfrom()");
            buffer[n] = '\0';
            printv("received message from pd");

            n = sendto(fd_udp, applyCommand(buffer), 32, 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if (n == -1) printError("main: sendto()");
        } 
        if (FD_ISSET(fd_udp_client, &rset)) {
            printv("estou aqui\n"); // DEBUG
            n = recvfrom(fd_udp_client, reply, 32, 0, (struct sockaddr*) &addr_udp_client, &addrlen_udp_client);
            if (n == -1) printError("request: recvfrom()");
            reply[n] = '\0';

            confirm_validation(reply);
        }
        if (FD_ISSET(fd_tcp, &rset)) {
            printv("entrei"); // DEBUG
            for (int i=0; i<numClients; i++) {
                if (fd_array[i].fd == -5) {
                    if ((fd_array[i].fd = accept(fd_tcp, (struct sockaddr *) &addr_tcp, &addrlen_tcp)) == -1) printError("main: accept()");
                    tcp_flag = 1;
                    break;
                }
            }

            if(tcp_flag == 0) {
                if ((fd_array[numClients++].fd = accept(fd_tcp, (struct sockaddr *) &addr_tcp, &addrlen_tcp)) == -1) printError("main: accept()");
                fd_array = (tcp_client*)realloc(fd_array, sizeof(struct tcp_client) * (numClients + 1));
                printf("%d\n", numClients); // DEBUG
            }
            else tcp_flag = 0;
            // userSession(fd_array[numClients - 1]);
        }
    
        for (i=0; i<numClients; i++) {
            if (FD_ISSET(fd_array[i].fd, &rset)) {
                userSession(i);
            }
        }
    }
    
    //closeConnection(fd_udp, res_udp);

    return 0;
}



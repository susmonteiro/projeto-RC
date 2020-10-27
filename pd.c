/*  Personal Device Application (PD)

invoked with: 
    ./pd PDIP [-d PDport] [-n ASIP] [-p ASport]

Once the PD program is running, it waits for a registration command from the user.
Then it waits for validation codes (VC) sent by the AS, which should be displayed. 
The PD application can also receive a command to exit, unregistering the user.
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include "config.h"
#include "connection.c"
#include "error.c"

#define MAXARGS 8
#define MINARGS 2

#define MAX(a, b) a*(a>b) + b*(b>=a)

void fdManager();

int fd_udp_client, fd_udp;
fd_set rset;
struct addrinfo hints_udp_client, *res_udp_client, hints_udp, *res_udp;
socklen_t addrlen_udp;
struct sockaddr_in addr_udp;

char pdip[32], pdport[8], asip[32], asport[8];
char command[6], uid[7], pass[10], buffer[32];
int maxfdp1;

void unregistration() {
    freeaddrinfo(res_udp_client);
    close(fd_udp_client);
    exit(0);
}

void endPD() {
    unregistration();
}

void registration(char* uid, char* pass) {
    int n, len;
    char message[64], reply[9];

    if (strlen(uid) != 5 || strlen(pass) != 8) {
        printf("ERR2\n");   /* debug */ // TODO change this
        return;
    }

    len = sprintf(message, "REG %s %s %s %s\n", uid, pass, pdip, pdport);
    if (len < 0) errorExit("sprintf()");
    
    printf("our message: %s", message);
    n = sendto(fd_udp_client, message, len*sizeof(char), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) errorExit("sendto()");

    n = recvfrom(fd_udp_client, reply, 9, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
    if (n == -1) errorExit("recvfrom()");
    reply[n] = '\0';

    printf("server reply: %s\n", reply); /* debug */ // TODO remove this

    if (!strcmp(reply, "RRG OK\n"))
        printf("Registration successful\n");
    else if (!strcmp(reply, "RRG NOK\n"))
        printf("Registration was a failureeee you a failureeee\n"); // TODO change this
    else
        endPD();
}

char * validateRequest(char* message) {
    char command[5], uid[32], vc[32], fname[32], type[32];
    char op;
    printf("message from AS: %s", message);
    sscanf(message, "%s %s %s %c", command, uid, vc, &op);
    if (!strcmp(command, "VLC") && strlen(vc) == 4) {   
        switch(op) {
        case 'L':
            strcpy(type, "list");
            break;
        case 'D':
            strcpy(type, "delete");
            break;
        case 'R':
            strcpy(type, "retrieve");
            break;
        case 'U':
            strcpy(type, "upload");
        case 'X':
            strcpy(type, "delete");
        }
        if (op == 'R' || op == 'U' || op == 'L' || op == 'D') {
            sscanf(message, "%s %s %s %c %s", command, uid, vc, &op, fname);
            sprintf(message, "VC=%s, %s:%s\n", vc, type, fname);
        }
        else {
            sscanf(message, "%s %s %s %c", command, uid, vc, &op);
            sprintf(message, "VC=%s, %s\n", vc, type); 
        }
        return "RVC OK\n";
    } else {
        return "ERR\n";
    }
}

void snooze(){
    printf("alarm out\n");  /* debug */
    fdManager();
}


void fdManager() {
    int n; 
    while(1) {
        FD_ZERO(&rset); 
        FD_SET(STDIN, &rset);
        FD_SET(fd_udp, &rset);

        maxfdp1 = MAX(STDIN, fd_udp) + 1;

        n = select(maxfdp1, &rset, NULL, NULL, NULL);
        if (n == -1) errorExit("select()");

        if (FD_ISSET(fd_udp, &rset)) {              // server of AS
            n = recvfrom(fd_udp, buffer, 32, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
            if (n == -1) errorExit("recvfrom()");
            buffer[n] = '\0';
            printf("received message from as: %s\n", buffer);

            n = sendto(fd_udp, validateRequest(buffer), n, 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if (n == -1) errorExit("sendto()");
        }
        
        if (FD_ISSET(STDIN, &rset)) {
            signal(SIGALRM, snooze);
            alarm(2);
            scanf("%s", command);
            if (!strcmp(command, "reg")) {
                scanf("%s %s", uid, pass);
                registration(uid, pass);
            } else if (!strcmp(command, "exit")) {
                unregistration();
            } else printf("wrong command\n");
            alarm(0);
        }
    }
}



int main(int argc, char* argv[]) {
    int i;
    
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

    udpOpenConnection(pdport, &fd_udp, &res_udp);
    addrlen_udp = sizeof(addr_udp);
    udpConnect(asip, asport, &fd_udp_client, &res_udp_client);

    signal(SIGINT, endPD);



    fdManager();

    return 0;
}
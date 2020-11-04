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
#include "connection.h"
#include "error.h"

#define MAXARGS 8
#define MINARGS 2

#define NO_MSG 0
#define TYPE_REG 1
#define TYPE_END 2

#define MAX(a, b) a*(a>b) + b*(b>=a)

void fdManager();

int fd_udp_client, fd_udp;
fd_set rset;
struct addrinfo hints_udp_client, *res_udp_client, hints_udp, *res_udp;
socklen_t addrlen_udp;
struct sockaddr_in addr_udp;

int numTries = 0;
int typeMessage = NO_MSG;
char lastMessage[128];

char pdip[32], pdport[8], asip[32], asport[8];
char command[6], uid[7], pass[10], buffer[32];
int maxfdp1;

void resetLastMessage() {
    typeMessage = NO_MSG;
    lastMessage[0] = '\0';
    numTries = 0;
    alarm(0);
}

void unregistration() {
    int n, len;
    char message[64];

    len = sprintf(message, "UNR %s %s\n", uid, pass);
    if (len < 0) errorExit("sprintf()");

    n = sendto(fd_udp_client, message, len*sizeof(char), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) errorExit("sendto()");
    strcpy(lastMessage, message);
    typeMessage = TYPE_END;
    alarm(2);
}

void endPD() {
    unregistration();
    printf("Exiting...\n");
    freeaddrinfo(res_udp_client);
    close(fd_udp_client);
    exit(0);
}

void registration(char* uid, char* pass) {
    int n, len;
    char message[64];

    if (strlen(uid) != 5 || strlen(pass) != 8) {
        printf("Error: invalid arguments\n");
        return;
    }

    len = sprintf(message, "REG %s %s %s %s\n", uid, pass, pdip, pdport);
    if (len < 0) errorExit("sprintf()");
    
    //printf("our message: %s", message); // DEBUG
    n = sendto(fd_udp_client, message, len*sizeof(char), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) errorExit("sendto()");
    strcpy(lastMessage, message);
    typeMessage = TYPE_REG;
    alarm(2);
}

char * validateRequest(char* message) {
    char command[5], uid[32], vc[32], fname[32], type[32];
    char* res;
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
            break;
        case 'X':
            strcpy(type, "delete");
            break;
        }
        if (op == 'R' || op == 'U' || op == 'D') {
            sscanf(message, "%s %s %s %c %s", command, uid, vc, &op, fname);
            sprintf(message, "VC=%s, %s:%s\n", vc, type, fname);
            printf("%s", message);
        }
        else {
            sscanf(message, "%s %s %s %c", command, uid, vc, &op);
            sprintf(message, "VC=%s, %s\n", vc, type);
            printf("%s", message);
        }
        res = (char*)malloc(32*sizeof(char));
        sprintf(res, "RVC %s OK\n", uid);
        printf("%s", res);
        return res;   // TODO tid
    } else {
        return "ERR\n";
    }
}

void fdManager() {
    int n; 
    char reply[9];

    while(1) {
        FD_ZERO(&rset); 
        FD_SET(STDIN, &rset);
        FD_SET(fd_udp, &rset);
        FD_SET(fd_udp_client, &rset);

        maxfdp1 = MAX(STDIN, fd_udp);
        maxfdp1 = MAX(maxfdp1, fd_udp_client) + 1;

        n = select(maxfdp1, &rset, NULL, NULL, NULL);
        if (n == -1) {
            printError("select(): interrupted by signal"); // TODO change this
            continue;
        }

        if (FD_ISSET(STDIN, &rset)) {
            if (typeMessage == NO_MSG) {     // reads message if there are none waiting to be acknowledge
                scanf("%s", command);
                if (!strcmp(command, "reg")) {
                    scanf("%s %s", uid, pass);
                    registration(uid, pass);
                } else if (!strcmp(command, "exit")) {
                    unregistration();
                } else printf("Error: invalid command\n");
            }
        }

        if (FD_ISSET(fd_udp, &rset)) {              // server of AS
            n = recvfrom(fd_udp, buffer, 32, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
            if (n == -1) errorExit("recvfrom()");
            buffer[n] = '\0';
            //printf("received message from as: %s\n", buffer);

            n = sendto(fd_udp, validateRequest(buffer), 32, 0, (struct sockaddr*) &addr_udp, addrlen_udp);
            if (n == -1) errorExit("sendto()");
        }

        if (FD_ISSET(fd_udp_client, &rset)) {
            n = recvfrom(fd_udp_client, reply, 9, 0, (struct sockaddr*) &addr_udp, &addrlen_udp);
            if (n == -1) errorExit("recvfrom()");
            reply[n] = '\0';

            //printf("server reply: %s", reply);  /* debug */ // TODO remove this

            if (!strcmp(reply, "RRG OK\n") && typeMessage == TYPE_REG) {
                resetLastMessage();
                printf("Registration successful\n");
            } else if (!strcmp(reply, "RRG NOK\n") && typeMessage == TYPE_REG) {
                resetLastMessage();
                printf("Error: registration successful\n");
            } else if (!strcmp(reply, "RUN OK\n") && typeMessage == TYPE_END) {
                resetLastMessage();
                printf("Unregistration successful\nExiting...\n");
                endPD();
            } else if (!strcmp(reply, "RUN NOK\n") && typeMessage == TYPE_END) {
                resetLastMessage();
                printf("Error: unregistration unsuccessful\nExiting...\n");
            } else {
                printf("Error: unexpected answer from AS\n");
            }
        } 
    }
}

void resendMessage() {
    int n;
    if (numTries++ > 2) endPD();
    n = sendto(fd_udp_client, lastMessage, strlen(lastMessage)*sizeof(char), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) errorExit("sendto()");
    printf("Resending last message to AS...\n");
    alarm(2);
}



int main(int argc, char* argv[]) {
    int i;
    
    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s PDIP [-d PDport] [-n ASIP] [-p ASport]\n", argv[0]);
        errorDIYexit("incorrect number of arguments");
    }

    strcpy(pdip, argv[1]);
    strcpy(pdport, PDPORT);
    strcpy(asip, ASIP);
    strcpy(asport, ASPORT);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
           printf("​Usage: %s PDIP [-d PDport] [-n ASIP] [-p ASport]\n", argv[0]); 
           exit(0);
        } else if (!strcmp(argv[i], "-d")) {
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
    signal(SIGALRM, resendMessage);

    fdManager();

    return 0;
}

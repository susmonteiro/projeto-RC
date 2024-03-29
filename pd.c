/*  Personal Device Application (PD)

invoked with: 
    ./pd PDIP [-d PDport] [-n ASIP] [-p ASport]

Once the PD program is running, it waits for a registration command from the user.
Then it waits for validation codes (VC) sent by the AS, which should be displayed. 
The PD application can also receive a command to exit, unregistering the user.
*/

#include "config.h"
#include "connection.h"
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
#include <unistd.h>

#define MAXARGS 8
#define MINARGS 2

#define NO_MSG 0
#define TYPE_REG 1
#define TYPE_END 2

#define NOT_REGISTERED 0
#define REGISTERED 1

#define MAX(a, b) a *(a > b) + b *(b >= a)

void fdManager();
void freePD();
void unregistration();

int fd_udp_client, fd_udp;
fd_set rset;
struct addrinfo hints_udp_client, *res_udp_client, hints_udp, *res_udp;
socklen_t addrlen_udp;
struct sockaddr_in addr_udp;

int numTries = 0;
int typeMessage = NO_MSG;
char lastMessage[128];
int messageToResend = FALSE;
int endPD = FALSE;

char pdip[32], pdport[8], asip[32], asport[8];
char uid[6], pass[9];
int maxfdp1, registered = NOT_REGISTERED;

/*      === error functions ===       */

void errorExit(char *errorMessage) {
    if (errno != 0)
        printf("ERR: %s: %s\n", errorMessage, strerror(errno));
    else
        printf("ERR: %s\n", errorMessage);
    freePD();
}

/*      === sighandler functions ===       */

void resendHandler() {
    messageToResend = TRUE;
}

void exitHandler() {
    endPD = TRUE;
}

/*      === resend messages that were not acknowledge ===       */

void resetLastMessage() {
    typeMessage = NO_MSG;
    lastMessage[0] = '\0';
    numTries = 0;
    alarm(0);
}

void resendMessage() {
    int n;
    messageToResend = FALSE;
    if (numTries++ > 2) freePD();
    n = sendto(fd_udp_client, lastMessage, strlen(lastMessage) * sizeof(char), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) errorExit("sendto()");
    alarm(2);
}

/*      === end PD ===       */

void freePD() {
    printf("Exiting...\n");
    close(fd_udp);
    freeaddrinfo(res_udp);
    close(fd_udp_client);
    freeaddrinfo(res_udp_client);
    exit(0);
}

void exitPD() {
    unregistration();
    freePD();
}

/*      === command functions ===        */

void registration(char *tmpUid, char *tmpPass) {
    // reg UID pass
    int n, len;
    char message[64];

    scanf("%s %s", tmpUid, tmpPass);

    if (strlen(tmpUid) != 5 || strlen(tmpPass) != 8) {
        printf("Error: invalid arguments\n");
        return;
    }

    if (registered == REGISTERED) {
        printf("Error: this pd is already registered\n");
        return;
    }

    len = sprintf(message, "REG %s %s %s %s\n", tmpUid, tmpPass, pdip, pdport);
    if (len < 0) errorExit("sprintf()");

    n = sendto(fd_udp_client, message, len * sizeof(char), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) errorExit("sendto()");
    strcpy(lastMessage, message);
    typeMessage = TYPE_REG;
    alarm(2);
}

void unregistration() {
    // exit
    int n, len;
    char message[64];

    if (registered == NOT_REGISTERED) freePD();

    len = sprintf(message, "UNR %s %s\n", uid, pass);
    if (len < 0) errorExit("sprintf()");

    n = sendto(fd_udp_client, message, len * sizeof(char), 0, res_udp_client->ai_addr, res_udp_client->ai_addrlen);
    if (n == -1) errorExit("sendto()");
    strcpy(lastMessage, message);
    typeMessage = TYPE_END;
    alarm(2);
}

void validateRequest(char *message) {
    char command[5], uid[32], vc[32], fname[32], type[32], result[64];
    char op;
    int n;

    sscanf(message, "%s %s %s %c", command, uid, vc, &op);
    if (!strcmp(command, "VLC") && strlen(vc) == 4) {
        switch (op) {
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
            printf("Operation: %s %s\nVC = %s\n", type, fname, vc);
        } else {
            sscanf(message, "%s %s %s %c", command, uid, vc, &op);
            printf("Operation: %s\nVC = %s\n", type, vc);
        }
        sprintf(result, "RVC %s OK\n", uid);
    } else {
        strcpy(result, "ERR\n");
    }
    n = sendto(fd_udp, result, strlen(result) * sizeof(char), 0, (struct sockaddr *)&addr_udp, addrlen_udp);
    if (n == -1) errorExit("sendto()");
}

/*      === main code ===        */

void fdManager() {
    int n;
    char reply[32], buffer[64], command[6], tmpUid[7], tmpPass[10];

    while (1) {
        FD_ZERO(&rset);
        FD_SET(STDIN, &rset);
        FD_SET(fd_udp, &rset);
        FD_SET(fd_udp_client, &rset);

        maxfdp1 = MAX(STDIN, fd_udp);
        maxfdp1 = MAX(maxfdp1, fd_udp_client) + 1;

        n = select(maxfdp1, &rset, NULL, NULL, NULL);
        if (n == -1) { // if interrupted by signals
            if (endPD) exitPD();
            if (messageToResend) resendMessage();
            continue;
        }

        if (FD_ISSET(STDIN, &rset)) {
            if (typeMessage == NO_MSG) { // reads message if there are none waiting to be acknowledge
                scanf("%s", command);
                if (!strcmp(command, "reg")) {
                    registration(tmpUid, tmpPass);
                } else if (!strcmp(command, "exit")) {
                    unregistration();
                } else
                    printf("Error: invalid command\n");
            }
        }

        if (FD_ISSET(fd_udp, &rset)) { // server of AS
            n = recvfrom(fd_udp, buffer, 64, 0, (struct sockaddr *)&addr_udp, &addrlen_udp);
            if (n == -1) errorExit("recvfrom()");
            buffer[n] = '\0';
            validateRequest(buffer);
        }

        if (FD_ISSET(fd_udp_client, &rset)) {
            n = recvfrom(fd_udp_client, reply, 9, 0, (struct sockaddr *)&addr_udp, &addrlen_udp);
            if (n == -1) errorExit("recvfrom()");
            reply[n] = '\0';

            if (!strcmp(reply, "RRG OK\n") && typeMessage == TYPE_REG) {
                resetLastMessage();
                printf("Registration successful\n");
                strcpy(uid, tmpUid);
                strcpy(pass, tmpPass);
                registered = REGISTERED;
            } else if (!strcmp(reply, "RRG NOK\n") && typeMessage == TYPE_REG) {
                resetLastMessage();
                printf("Error: Registration unsuccessful\n");
            } else if (!strcmp(reply, "RUN OK\n") && typeMessage == TYPE_END) {
                resetLastMessage();
                printf("Unregistration successful\n");
                freePD();
            } else if (!strcmp(reply, "RUN NOK\n") && typeMessage == TYPE_END) {
                resetLastMessage();
                printf("Error: Unregistration unsuccessful\n");
            } else {
                printf("Error: Unexpected answer from AS\n");
                exitPD();
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int i;

    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s PDIP [-d PDport] [-n ASIP] [-p ASport]\n", argv[0]);
        errorExit("Error: incorrect number of arguments");
    }

    strcpy(pdip, argv[1]);
    strcpy(pdport, PDPORT);
    strcpy(asip, ASIP);
    strcpy(asport, ASPORT);

    for (i = MINARGS; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || i + 1 >= argc) {
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

    signal(SIGINT, exitHandler);
    signal(SIGALRM, resendHandler);

    fdManager();

    return 0;
}

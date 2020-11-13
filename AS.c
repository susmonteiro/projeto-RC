#include "config.h"
#include "connection.h"
#include "error.h"
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
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAXARGS 4
#define MINARGS 1

#define TRUE 1
#define FALSE 0

#define MAX(a, b) a *(a > b) + b *(b >= a)

typedef struct user {
    int fd;
    char uid[7];
} * User;

typedef struct request {
    char uid[7];
    char rid[5];
    char tid[5];
    char vc[5];
    char fop[2];
    char fname[32];
} * Request;

int numClients = 1;
int numRequests = 1;
int verbose = FALSE;

int fd_udp, fd_tcp;
User *users;
Request *requests;
fd_set rset;
socklen_t addrlen_udp, addrlen_tcp;
struct addrinfo hints_udp, *res_udp, hints_tcp, *res_tcp;
struct sockaddr_in addr_udp, addr_tcp;
int connected = FALSE;

char asport[8];

// print if verbose mode
void printv(char *message) {
    if (verbose) printf("%s\n", message);
}

char *login(int ind, char *uid, char *pass) {
    char message[64], path[64], currentPass[32];
    FILE *file;

    sprintf(path, "USERS/UID%s/UID%s_pass.txt", uid, uid);
    if (access(path, F_OK) != -1) {
        file = fopen(path, "r");
        fscanf(file, "%s", currentPass);
        fclose(file);
        if (strcmp(currentPass, pass))
            return "RLO NOK\n";
    }

    sprintf(path, "USERS/UID%s/UID%s_login.txt", uid, uid);
    file = fopen(path, "w");
    fclose(file);

    printv(uid);
    strcpy(users[ind]->uid, uid);
    users[ind]->uid[5] = '\0';
    printv(uid);
    sprintf(message, "User: login ok, UID=%s", uid);
    printv(message);
    return "RLO OK\n";
}

char *request(int ind, char *uid, char *rid, char *fop, char *fname) {
    char buffer[64], path[64], pdport[32], pdip[32], vc[5];
    int n, i, fd;
    FILE *file;
    struct addrinfo *res;

    if (strcmp(fop, "F") && strcmp(fop, "R") && strcmp(fop, "U") && strcmp(fop, "D") && strcmp(fop, "X") && strcmp(fop, "L"))
        return "RRQ EFOP\n";

    sprintf(path, "USERS/UID%s/UID%s_login.txt", uid, uid);
    if (access(path, F_OK) == -1)
        return "RRQ ELOG\n";

    if (strcmp(users[ind]->uid, uid))
        return "RRQ EUSER\n";

    sprintf(path, "USERS/UID%s/UID%s_reg.txt", uid, uid);
    if ((file = fopen(path, "r")) == NULL)
        return "RRQ EPD\n";
    fscanf(file, "%s\n%s", pdip, pdport);
    fclose(file);

    printf("%s %s\n", pdip, pdport);

    udpConnect(pdip, pdport, &fd, &res);

    sprintf(vc, "%04d", rand() % 10000);

    if (fname[0] == '\0') {
        sprintf(buffer, "User: %s req, UID=%s, RID=%s VC=%s", fop, uid, rid, vc);
        printv(buffer);
        sprintf(buffer, "VLC %s %s %s\n", uid, vc, fop);
    } else {
        sprintf(buffer, "User: %s req, UID=%s file:%s, RID=%s VC=%s", fop, uid, fname, rid, vc);
        printv(buffer);
        sprintf(buffer, "VLC %s %s %s %s\n", uid, vc, fop, fname);
    }

    n = sendto(fd, buffer, strlen(buffer), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) printError("request: sendto()");
    // TODO retry and return EPD if failed

    recvfrom(fd, buffer, 32, 0, (struct sockaddr *)&addr_udp, &addrlen_udp);

    if (!strcmp(buffer, "RVC NOK\n"))
        return "RRQ EPD\n";

    for (i = 0; i < numRequests; i++) {
        if (requests[i] == NULL) {
            requests[i] = (Request)malloc(sizeof(struct request));
            strcpy(requests[i]->rid, rid);
            strcpy(requests[i]->uid, uid);
            strcpy(requests[i]->vc, vc);
            strcpy(requests[i]->fname, fname);
            strcpy(requests[i]->fop, fop);
            break;
        }
    }
    if (i == numRequests) {
        numRequests++;
        requests = (Request *)realloc(requests, sizeof(Request) * (numRequests));
        requests[numRequests-1] = NULL;
    }

    close(fd);
    freeaddrinfo(res);

    return "RRQ OK\n";
}

char *secondAuth(char *uid, char *rid, char *vc) {
    int i;
    char message[64], tid[5];
    char *buffer;

    buffer = (char *)malloc(sizeof(char) * 32);

    for (i = 0; i < numRequests; i++) {
        if (!strcmp(requests[i]->rid, rid)) {
            break;
        }
    }

    if (i == numRequests || strcmp(requests[i]->vc, vc))
        return "RAU 0\n";

    sprintf(tid, "%04d", rand() % 10000);
    strcpy(requests[i]->tid, tid);

    sprintf(message, "User: UID=%s", uid);
    printv(message);
    if (!strcmp(requests[i]->fop, "R") || !strcmp(requests[i]->fop, "U") || !strcmp(requests[i]->fop, "D")) {
        sprintf(message, "%s, %s, TID=%s", requests[i]->fop, requests[i]->fname, tid);
        printv(message);
    } else {
        sprintf(message, "%s, TID=%s", requests[i]->fop, tid);
        printv(message);
    }

    sprintf(buffer, "RAU %s\n", tid);
    return buffer;
}

void userSession(int ind) {
    int n;
    char buffer[128], msg[256], path[64], command[5], uid[32], rid[32], fop[32], vc[32], fname[32];

    n = read(users[ind]->fd, buffer, 128);
    if (n == -1) {
        printError("userSession: read()");
    } else if (n == 0) {
        printf("entrei\n");
        if (strlen(users[ind]->uid) > 0) {
            sprintf(path, "USERS/UID%s/UID%s_login.txt", users[ind]->uid, users[ind]->uid);
            remove(path);
        }
        close(users[ind]->fd);
        users[ind] = NULL;
        return;
    }

    buffer[n] = '\0';
    sprintf(msg, "message from User: %s", buffer);
    printv(msg);

    sscanf(buffer, "%s", command);
    if (!strcmp(command, "LOG")) {
        sscanf(buffer, "%s %s %s", command, uid, rid);
        strcpy(buffer, login(ind, uid, rid));
    } else if (!strcmp(command, "REQ")) {
        sscanf(buffer, "%s %s %s %s %s", command, uid, rid, fop, fname);
        strcpy(buffer, request(ind, uid, rid, fop, fname));
    } else if (!strcmp(command, "AUT")) {
        sscanf(buffer, "%s %s %s %s", command, uid, rid, vc);
        strcpy(buffer, secondAuth(uid, rid, vc));
    }
    n = write(users[ind]->fd, buffer, strlen(buffer));
    if (n == -1) printError("userSession: write()");
}

char *registration(char *uid, char *pass, char *pdip_new, char *pdport_new) {
    char message[64], path[64], currentPass[32];
    FILE *file;

    sprintf(path, "USERS/UID%s/UID%s_pass.txt", uid, uid);
    if (access(path, F_OK) != -1) {
        file = fopen(path, "r");
        fscanf(file, "%s", currentPass);
        fclose(file);
        if (strcmp(currentPass, pass))
            return "RRG NOK\n";
    } else {
        sprintf(path, "USERS/UID%s", uid);
        mkdir(path, 0777);
        sprintf(path, "USERS/UID%s/UID%s_pass.txt", uid, uid);
        file = fopen(path, "w");
        fprintf(file, "%s", pass);
        fclose(file);
    }

    sprintf(path, "USERS/UID%s/UID%s_reg.txt", uid, uid);
    file = fopen(path, "w");
    fprintf(file, "%s\n%s", pdip_new, pdport_new);
    fclose(file);
    sprintf(message, "PD: new User, UID = %s", uid);
    printv(message);
    return "RRG OK\n";
}

char *unregistration(char *uid, char *pass) {
    char path[64], currentPass[9];
    FILE *file;

    sprintf(path, "USERS/UID%s/UID%s_pass.txt", uid, uid);
    if (access(path, F_OK) != -1) {
        file = fopen(path, "r");
        fscanf(file, "%s", currentPass);
        fclose(file);
        if (strcmp(currentPass, pass))
            return "RUN NOK\n";
    }

    sprintf(path, "USERS/UID%s/UID%s_reg.txt", uid, uid);
    remove(path);
    return "RUN OK\n";
}

char *validateOperation(char *uid, char *tid) {
    // message - VLD UIF TID
    int i, j;
    char message[128], error[128];
    char *reply;
    char fop = 'A';

    printf("inside validate operation\n");

    reply = (char *)malloc(128 * sizeof(char));

    for (i = 0; i < numRequests; i++) {
        if (requests[i] != NULL && !strcmp(tid, requests[i]->tid))
            break;
    }
    if (i == numRequests) {
        sprintf(error, "Error: Request was not found for TID=%s", tid);
        printv(error);
        fop = 'E';
    }
    for (j = 0; j < numClients; j++) {
        if (!strcmp(users[j]->uid, uid)) {
            break;
        }
    }
    if (j == numClients) {
        sprintf(error, "Error: Request was not found for UID=%s", uid);
        printv(error);
        for (j = 0; j < numClients; j++) {
            if (!strcmp(users[j]->uid, requests[i]->uid)) {
                fop = 'E';
                break;
            }
        }
    }

    if (fop != 'E') fop = requests[i]->fop[0];

    if (fop == 'R' || fop == 'U' || fop == 'D') {
        sprintf(message, "User: UID=%s %c %s, TID=%s", uid, fop, requests[i]->fname, tid);
        printv(message);
        sprintf(reply, "CNF %s %s %c %s\n", uid, tid, fop, requests[i]->fname);
    } else {
        sprintf(message, "User: UID=%s %c TID=%s", uid, fop, tid);
        printv(message);
        sprintf(reply, "CNF %s %s %c\n", uid, tid, fop);
    }
    requests[i] = NULL;
    return reply;
}

char *applyCommand(char *message) {
    char command[5], arg1[32], arg2[32], arg3[32], arg4[32];
    char msg[64];
    printf("inside applyCommand\n");
    sprintf(msg, "message from PD or FS: %s", message);
    printv(msg);
    sscanf(message, "%s %s %s %s %s", command, arg1, arg2, arg3, arg4);
    if (!strcmp(command, "REG")) {
        return registration(arg1, arg2, arg3, arg4);
    } else if (!strcmp(command, "UNR")) {
        return unregistration(arg1, arg2);
    } else if (!strcmp(command, "VLD")) {
        printf("going to validate operation\n");
        return validateOperation(arg1, arg2);
    } else {
        return "ERR\n";
    }
}

void freePDconnection() {
    printf("Exiting...\n");
    freeaddrinfo(res_udp);
    close(fd_udp);
    exit(0);
}

void endAS() {
    int i = 0;
    // freeaddrinfo(res_udp_client);
    // close(fd_udp_client);
    freeaddrinfo(res_tcp);
    close(fd_tcp);
    for (i = 0; i < numClients; i++) {
        if (users[i] != NULL) {
            close(users[i]->fd);
        }
    }
    freePDconnection();
}

int main(int argc, char *argv[]) {
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
        } else if (!strcmp(argv[i], "-h") || i + 1 == argc) {
            printf("​Usage: %s -p [ASport] [-v]\n", argv[0]);
            exit(0);
        } else if (!strcmp(argv[i], "-p")) {
            if (i + 1 == argc) {
                printf("​Usage: %s -p [ASport] [-v]\n", argv[0]);
                printError("incorrect number of arguments");
            }
            strcpy(asport, argv[++i]);
        }
    }

    srand(time(NULL));

    mkdir("USERS", 0777);

    tcpOpenConnection(asport, &fd_tcp, &res_tcp);
    if (listen(fd_tcp, 5) == -1) printError("TCP: listen()");

    users = (User *)malloc(sizeof(User));
    users[0] = NULL;

    requests = (Request *)malloc(sizeof(Request));
    requests[0] = NULL;

    udpOpenConnection(asport, &fd_udp, &res_udp);
    connected = TRUE;
    addrlen_udp = sizeof(addr_udp);

    signal(SIGINT, endAS);

    while (1) {
        FD_ZERO(&rset);
        FD_SET(fd_udp, &rset);
        // FD_SET(fd_udp_client, &rset);
        FD_SET(fd_tcp, &rset);

        for (i = 0; i < numClients; i++) {
            if (users[i] != NULL)
                FD_SET(users[i]->fd, &rset);
        }

        // maxfdp1 = MAX(MAX(fd_tcp, fd_udp), fd_udp_client) + 1;
        maxfdp1 = MAX(fd_tcp, fd_udp) + 1;

        for (i = 0; i < numClients; i++) {
            if (users[i] != NULL)
                maxfdp1 = MAX(maxfdp1, users[i]->fd) + 1;
        }

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd_udp, &rset)) {
            n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr *)&addr_udp, &addrlen_udp);
            if (n == -1) printError("main: recvfrom()");
            buffer[n] = '\0';

            n = sendto(fd_udp, applyCommand(buffer), 32, 0, (struct sockaddr *)&addr_udp, addrlen_udp);
            if (n == -1) printError("main: sendto()");
        }
        if (FD_ISSET(fd_tcp, &rset)) {
            for (i = 0; i < numClients; i++) {
                if (users[i] == NULL) {
                    users[i] = (User)malloc(sizeof(struct user));
                    if ((users[i]->fd = accept(fd_tcp, (struct sockaddr *)&addr_tcp, &addrlen_tcp)) == -1) printError("main: accept()");
                    break;
                }
            }
            if (i == numClients) {
                numClients++;
                users = (User *)realloc(users, sizeof(User) * (numClients));
                users[numClients-1] = NULL;
            }
        }
        for (i = 0; i < numClients; i++) {
            if (FD_ISSET(users[i]->fd, &rset)) {
                userSession(i);
            }
        }
    }

    return 0;
}

#include "config.h"
#include "connection.h"
#include <arpa/inet.h>
#include <dirent.h>
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

#define MAX(a, b) a *(a > b) + b *(b >= a)

void exitAS();

typedef struct request {
    char uid[7];
    char rid[5];
    char tid[5];
    char vc[5];
    char fop[2];
    char fname[32];
} * Request;

typedef struct user {
    int fd;
    char uid[7];
    int numTries;
    int confirmationPending;
    char lastMessage[128];
    int pd_fd;
    struct addrinfo *pd_res;
    struct request pendingRequest;
} * User;

int numClients = 0;
int numRequests = 0;
int verbose = FALSE;
int messageToResend = FALSE;
int endAS;

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
    if (verbose)
        printf("%s\n", message);
}

/*      === error functions ===       */

void errorExit(char *errorMessage) {
    if (errno != 0)
        printf("ERR: %s: %s\n", errorMessage, strerror(errno));
    else
        printf("ERR: %s\n", errorMessage);
    exitAS();
}

void printError(char *errorMessage) {
    if (errno != 0)
        printf("ERR: %s: %s\nNot exiting...\n", errorMessage, strerror(errno));
    else
        printf("ERR: %s\nNot exiting...\n", errorMessage);
}

/*      === sighandler functions ===       */

void resendHandler() {
    messageToResend = TRUE;
}

void exitHandler() {
    endAS = TRUE;
}

/*      === resend messages that were not acknowledge ===       */

void resetLastMessage(User user) {
    user->confirmationPending = FALSE;
    user->numTries = 0;
    user->lastMessage[0] = '\0';
}

void resendMessage() {
    int n, i;

    messageToResend = FALSE;

    for (i = 0; i < numClients; i++) {
        if (users[i] == NULL)
            continue;
        if (!users[i]->confirmationPending) {
            continue;
        }
        if (users[i]->numTries++ > 1) {
            n = write(users[i]->fd, "RRQ EPD\n", 8);
            if (n == -1)
                printError("resendMessage: write()");
            resetLastMessage(users[i]);
            continue;
        }
        n = sendto(users[i]->pd_fd, users[i]->lastMessage, strlen(users[i]->lastMessage) * sizeof(char), 0, users[i]->pd_res->ai_addr, users[i]->pd_res->ai_addrlen);
        if (n == -1)
            errorExit("sendto()");
        users[i]->numTries++;
    }
    alarm(2);
    return;
}

/*      === end AS ===       */

void freePDconnection() {
    printf("Exiting...\n");
    freeaddrinfo(res_udp);
    close(fd_udp);
    exit(0);
}

void exitAS() {
    int i = 0;
    freeaddrinfo(res_tcp);
    close(fd_tcp);
    for (i = 0; i < numClients; i++) {
        if (users[i] != NULL) {
            close(users[i]->fd);
        }
    }
    freePDconnection();
}

char *login(User userInfo, char *uid, char *pass) {
    char message[64], path[64], currentPass[32];
    FILE *file;

    sprintf(path, "USERS/UID%s/UID%s_pass.txt", uid, uid);
    if (access(path, F_OK) != -1) {
        file = fopen(path, "r");
        fscanf(file, "%s", currentPass);
        fclose(file);
        if (strcmp(currentPass, pass))
            return "RLO NOK\n";
    } else {
        return "RLO NOK\n";
    }

    sprintf(path, "USERS/UID%s/UID%s_login.txt", uid, uid);
    file = fopen(path, "w");
    fclose(file);

    strcpy(userInfo->uid, uid);
    userInfo->uid[5] = '\0';
    sprintf(message, "User: login ok, UID=%s", uid);
    printv(message);
    return "RLO OK\n";
}

void request(User userInfo, char *uid, char *rid, char *fop, char *fname) {
    char buffer[64], path[64], pdport[32], pdip[32], vc[5];
    int n, i;
    FILE *file;

    if (strcmp(fop, "E") && strcmp(fop, "R") && strcmp(fop, "U") && strcmp(fop, "D") && strcmp(fop, "X") && strcmp(fop, "L")) {
        n = write(userInfo->fd, "RRQ EFOP\n", 9);
        if (n == -1) printError("userSession: write()");
        close(userInfo->pd_fd);
        freeaddrinfo(userInfo->pd_res);
        return;
    }

    sprintf(path, "USERS/UID%s/UID%s_login.txt", uid, uid);
    if (access(path, F_OK) == -1) {
        n = write(userInfo->fd, "RRQ ELOG\n", 9);
        if (n == -1) printError("userSession: write()");
        close(userInfo->pd_fd);
        freeaddrinfo(userInfo->pd_res);
        return;
    }

    if (strcmp(userInfo->uid, uid)) {
        n = write(userInfo->fd, "RRQ EUSER\n", 10);
        if (n == -1) printError("userSession: write()");
        close(userInfo->pd_fd);
        freeaddrinfo(userInfo->pd_res);
        return;
    }

    sprintf(path, "USERS/UID%s/UID%s_reg.txt", uid, uid);
    if ((file = fopen(path, "r")) == NULL) {
        n = write(userInfo->fd, "RRQ EPD\n", 8);
        if (n == -1) printError("userSession: write()");
        close(userInfo->pd_fd);
        freeaddrinfo(userInfo->pd_res);
        return;
    }
    fscanf(file, "%s\n%s", pdip, pdport);
    fclose(file);

    udpConnect(pdip, pdport, &userInfo->pd_fd, &userInfo->pd_res);

    sprintf(vc, "%04d", rand() % 10000);

    if (!strcmp(fop, "L") || !strcmp(fop, "X") || !strcmp(fop, "E")) {
        sprintf(buffer, "User: %s req, UID=%s, RID=%s VC=%s", fop, uid, rid, vc);
        printv(buffer);
        sprintf(buffer, "VLC %s %s %s\n", uid, vc, fop);
    } else {
        sprintf(buffer, "User: %s req, UID=%s file:%s, RID=%s VC=%s", fop, uid, fname, rid, vc);
        printv(buffer);
        sprintf(buffer, "VLC %s %s %s %s\n", uid, vc, fop, fname);
    }

    for (i = 0; i < numRequests + 1; i++) {
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
        requests = (Request *)realloc(requests, sizeof(Request) * (numRequests + 1));
        requests[numRequests] = NULL;
    }

    n = sendto(userInfo->pd_fd, buffer, strlen(buffer), 0, userInfo->pd_res->ai_addr, userInfo->pd_res->ai_addrlen);
    if (n == -1)
        printError("request: sendto()");

    userInfo->confirmationPending = TRUE;
}

void requestReply(User userInfo) {
    char buffer[64], command[8], uid[8], status[8];
    int i, n, allGoodMyDude = FALSE;

    recvfrom(userInfo->pd_fd, buffer, 64, 0, (struct sockaddr *)&addr_udp, &addrlen_udp);

    sscanf(buffer, "%s %s %s", command, uid, status);

    if (strcmp("RVC", command)) {
        strcpy(buffer, "ERR\n");
    } else if (strcmp(userInfo->uid, uid)) {
        strcpy(buffer, "RRQ EUSER\n");
    } else if (!strcmp(status, "NOK")) {
        strcpy(buffer, "ERR\n");
    } else if (!strcmp(status, "OK")) {
        strcpy(buffer, "RRQ OK\n");
        allGoodMyDude = TRUE;
    } else {
        strcpy(buffer, "ERR\n");
    }

    if (!allGoodMyDude) {
        for (i = 0; i < numRequests + 1; i++) {
            if (!strcmp(requests[i]->uid, userInfo->uid)) {
                requests[i] = NULL;
                break;
            }
        }
    }

    resetLastMessage(userInfo);

    close(userInfo->pd_fd);
    freeaddrinfo(userInfo->pd_res);
    n = write(userInfo->fd, buffer, strlen(buffer));
    if (n == -1) printError("userSession: write()");
    return;
}

char *secondAuth(char *uid, char *rid, char *vc) {
    int i;
    char message[64], tid[5];
    char *buffer;

    buffer = (char *)malloc(sizeof(char) * 32);

    for (i = 0; i < numRequests + 1; i++) {
        if (requests[i] != NULL && !strcmp(requests[i]->rid, rid)) {
            break;
        }
    }

    if (i == numRequests + 1 || strcmp(requests[i]->vc, vc)) {
        return "RAU 0\n";
    }

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
    char buffer[128], msg[256], path[128], command[5], uid[32], rid[32], fop[32], vc[32], fname[32];

    n = read(users[ind]->fd, buffer, 128);
    if (n == -1) {
        printError("userSession: read()");
    } else if (n == 0) {
        close(users[ind]->fd);
        if (strlen(users[ind]->uid) > 0) {
            sprintf(path, "USERS/UID%s/UID%s_login.txt", users[ind]->uid, users[ind]->uid);
            remove(path);
        }

        users[ind] = NULL;
        return;
    }

    buffer[n] = '\0';
    sprintf(msg, "message from User: %s", buffer);
    printv(msg);

    sscanf(buffer, "%s", command);
    if (!strcmp(command, "LOG")) {
        sscanf(buffer, "%s %s %s", command, uid, rid);
        strcpy(buffer, login(users[ind], uid, rid));
        n = write(users[ind]->fd, buffer, strlen(buffer));
        if (n == -1)
            printError("userSession: write()");
    } else if (!strcmp(command, "REQ")) {
        sscanf(buffer, "%s %s %s %s %s", command, uid, rid, fop, fname);
        request(users[ind], uid, rid, fop, fname);
    } else if (!strcmp(command, "AUT")) {
        sscanf(buffer, "%s %s %s %s", command, uid, rid, vc);
        strcpy(buffer, secondAuth(uid, rid, vc));
        n = write(users[ind]->fd, buffer, strlen(buffer));
        if (n == -1)
            printError("userSession: write()");
    }
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
    char path[64], currentPass[32];
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
    char message[128], error[128], dirpath[64], filepath[512];
    char *reply;
    char fop = 'A';
    struct dirent *dir;
    DIR *d;

    reply = (char *)malloc(128 * sizeof(char));

    for (i = 0; i < numRequests + 1; i++) {
        if (requests[i] != NULL && !strcmp(tid, requests[i]->tid))
            break;
    }
    if (i == numRequests + 1) {
        sprintf(error, "Error: Request was not found for TID=%s", tid);
        printv(error);
        fop = 'E';
    }
    for (j = 0; j < numClients + 1; j++) {
        if (!strcmp(users[j]->uid, uid)) {
            break;
        }
    }
    if (j == numClients + 1) {
        sprintf(error, "Error: Request was not found for UID=%s", uid);
        printv(error);
        for (j = 0; j < numClients; j++) {
            if (!strcmp(users[j]->uid, requests[i]->uid)) {
                fop = 'E';
                break;
            }
        }
    }

    if (fop != 'E')
        fop = requests[i]->fop[0];

    if (fop == 'X') {
        sprintf(dirpath, "USERS/UID%s", requests[i]->uid);
        d = opendir(dirpath);
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                if (dir->d_name[0] != '.') {
                    sprintf(filepath, "%s/%s", dirpath, dir->d_name);
                    remove(filepath);
                }
            }
            closedir(d);
        }
        rmdir(dirpath);
    }

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
    sprintf(msg, "message from PD or FS: %s", message);
    printv(msg);
    sscanf(message, "%s %s %s %s %s", command, arg1, arg2, arg3, arg4);
    if (!strcmp(command, "REG")) {
        return registration(arg1, arg2, arg3, arg4);
    } else if (!strcmp(command, "UNR")) {
        return unregistration(arg1, arg2);
    } else if (!strcmp(command, "VLD")) {
        return validateOperation(arg1, arg2);
    } else {
        return "ERR\n";
    }
}

void fdManager() {
    char buffer[128];
    int i, n, maxfdp1, fd;
    while (1) {
        FD_ZERO(&rset);
        FD_SET(fd_udp, &rset);
        // FD_SET(fd_udp_client, &rset);
        FD_SET(fd_tcp, &rset);

        for (i = 0; i < numClients; i++) {
            if (users[i] != NULL) {
                FD_SET(users[i]->fd, &rset);
                if (users[i]->confirmationPending) FD_SET(users[i]->pd_fd, &rset);
            }
        }

        // maxfdp1 = MAX(MAX(fd_tcp, fd_udp), fd_udp_client) + 1;
        maxfdp1 = MAX(fd_tcp, fd_udp);

        for (i = 0; i < numClients; i++) {
            if (users[i] != NULL) {
                maxfdp1 = MAX(maxfdp1, users[i]->fd);
                if (users[i]->confirmationPending) {
                    maxfdp1 = MAX(maxfdp1, users[i]->pd_fd);
                }
            }
        }

        maxfdp1++;

        n = select(maxfdp1, &rset, NULL, NULL, NULL);
        if (n == -1) { // if interrupted by signals
            if (endAS) exitAS();
            if (messageToResend) resendMessage();
            continue;
        }

        if (FD_ISSET(fd_udp, &rset)) { // message from PD or FS
            n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr *)&addr_udp, &addrlen_udp);
            if (n == -1)
                printError("main: recvfrom()");
            buffer[n] = '\0';

            n = sendto(fd_udp, applyCommand(buffer), 32, 0, (struct sockaddr *)&addr_udp, addrlen_udp);
            if (n == -1)
                printError("main: sendto()");
        }

        if (FD_ISSET(fd_tcp, &rset)) { // receive user connections
            fd = accept(fd_tcp, (struct sockaddr *)&addr_tcp, &addrlen_tcp);
            if (fd == -1) printError("main: accept()");
            for (i = 0; i < numClients + 1; i++) {
                if (users[i] == NULL) {
                    users[i] = (User)malloc(sizeof(struct user));
                    users[i]->confirmationPending = FALSE;
                    users[i]->fd = fd;
                    strcpy(users[i]->uid, "");
                    break;
                }
            }
            if (i == numClients) {
                numClients++;
                users = (User *)realloc(users, sizeof(User) * (numClients + 1));
                users[numClients] = NULL;
            }
        }
        for (i = 0; i < numClients + 1; i++) {
            if (users[i] != NULL) {
                if (users[i]->confirmationPending && FD_ISSET(users[i]->pd_fd, &rset)) { // message from PD
                    requestReply(users[i]);
                }
                if (FD_ISSET(users[i]->fd, &rset)) {      // message from user
                    if (!users[i]->confirmationPending) { // does not read message if cannot communicate with PD
                        userSession(i);
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int i;

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
    if (listen(fd_tcp, 5) == -1)
        printError("TCP: listen()");

    users = (User *)malloc(sizeof(User));
    users[0] = NULL;

    requests = (Request *)malloc(sizeof(Request));
    requests[0] = NULL;

    udpOpenConnection(asport, &fd_udp, &res_udp);
    connected = TRUE;
    addrlen_udp = sizeof(addr_udp);

    signal(SIGINT, exitHandler);
    signal(SIGALRM, resendHandler);
    alarm(5);

    fdManager();

    return 0;
}

#include "config.h"
#include "connection.h"
#include "error.h"
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
#include <sys/types.h>
#include <unistd.h>

#define MAXARGS 8
#define MINARGS 1

#define MAX(a, b) a *(a > b) + b *(b >= a)

typedef struct user {
    int fd;
    char uid[7];
    DIR *dir;
} * User;

typedef struct transaction {
    char uid[7];
    char tid[5];
    char vc[5];
    char fop[2];
    char fname[32];
} * Transaction;

int numClients = 0;
int numTransactions = 0;
int verbose = FALSE;

int fd_udp, fd_tcp;
User *users;
Transaction *transactions;
fd_set rset;
socklen_t addrlen_udp, addrlen_tcp;
struct addrinfo hints_udp, *res_udp, hints_tcp, *res_tcp;
struct sockaddr_in addr_udp, addr_tcp;

char fsport[8], asport[8], asip[32];

// print if verbose mode
void printv(char *message) { // TODO move to common file
    if (verbose == TRUE) printf("%s\n", message);
}

void endFS() {
    int i;
    freeaddrinfo(res_udp);
    close(fd_udp);
    freeaddrinfo(res_tcp);
    close(fd_tcp);
    for (i = 0; i < numClients; i++)
        close(users[i]->fd);
    printv("Ending FS...\n");
    exit(0);
}

/*      === command functions ===        */

char *list(char *uid, char *tid) {
    // TO DO (Rodrigo)
    char message[128];
    sprintf(message, "list operation done for UID=%s", uid);
    printv(message);
    return "RLS N[fname fsize]\n";
}

void delete (int fd, char *uid, char *fname) {
    // TODO NOK, INV, ERR
    char message[128];
    int n;

    if (remove(fname) != 0) {
        n = write(fd, "RDL EOF\n", 8);
        if (n == -1) errorExit("write()");
        return;
    }

    sprintf(message, "file %s deleted for user %s", fname, uid);
    printv(message);
    n = write(fd, "RDL OK\n", 7);
    if (n == -1) errorExit("write()");
}

void retrieve(int fd, char *uid, char *fname) {
    // TODO RRT NOK, RTT INV
    int n, fsize;
    char message[128], buffer[128];
    FILE *file;

    if ((file = fopen(fname, "r")) == NULL) {
        sprintf(message, "Error: file %s from user %s does not exist", fname, uid);
        printv(message);
        n = write(fd, "RRT EOF\n", 8);
        if (n == -1) errorExit("write()");
        return;
    }

    fseek(file, 0, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    sprintf(message, "file %s retrieved for user %s", fname, uid);
    printv(message);

    sprintf(message, "RRT OK %d ", fsize);
    n = write(fd, message, strlen(message));
    if (n == -1) errorExit("write()");

    while (fgets(buffer, 128, (FILE *)file) != NULL) {
        n = write(fd, buffer, strlen(buffer));
        if (n == -1) errorExit("write()");
    }
    fclose(file);
}

void upload(int fd, char *uid, char *fname) {
    // TO DO (Rodrigo)
    char message[128];
    sprintf(message, "%s stored for UID=%s", fname, uid);
    printv(message);
    //return "RUP status\n";
}

char *removeAll(char *uid, char *tid) {
    // TO DO (Rodrigo)
    char message[128];
    sprintf(message, "operation remove done for UID=%s", uid);
    printv(message);
    return "RRM status\n";
}

/*      === user manager ===        */

void userSession(int ind) {
    int n, fd, i;
    char buffer[128], message[128], command[5], uid[32], tid[32], fname[32], fsize[32], data[32], type[32];
    char msg[256]; //DEBUG

    n = read(users[ind]->fd, buffer, 128);
    if (n == -1)
        printError("userSession: read()");
    else if (n == 0) {
        fd = users[ind]->fd;
        printf("vai fechar\n"); // DEBUG
        close(fd);
        users[ind] = NULL;
        return;
    }

    buffer[n] = '\0';

    // DEBUG
    sprintf(msg, "message from User: %s", buffer);
    printv(msg);
    //END OF DEBUG

    for (i = 0; i < numTransactions + 1; i++) {
        if (transactions[i] == NULL) {
            transactions[i] = (Transaction)malloc(sizeof(struct transaction));
            break;
        }
    }
    if (i == numClients)
        transactions = (Transaction *)realloc(transactions, sizeof(Transaction) * (++numClients));

    sscanf(buffer, "%s %s %s", command, uid, tid);
    strcpy(users[ind]->uid, uid);
    users[ind]->uid[5] = '\0';

    strcpy(transactions[i]->uid, uid);
    strcpy(transactions[i]->tid, tid);

    if (!strcmp(command, "RTV") || !strcmp(command, "DEL")) {
        if (!strcmp(command, "RTV")) {
            strcpy(type, "retrieve");
            strcpy(transactions[i]->fop, "R");

        } else if (!strcmp(command, "DEL")) {
            strcpy(type, "delete");
            strcpy(transactions[i]->fop, "D");
        }
        sscanf(buffer, "%s %s %s %s", command, uid, tid, fname);
        sprintf(buffer, "UID=%s: %s %s\n", uid, type, fname);

        strcpy(transactions[i]->fname, fname);

    } else if (!strcmp(command, "UPL")) {
        sscanf(buffer, "%s %s %s %s %s %s", command, uid, tid, fname, fsize, data);
        sprintf(buffer, "UID=%s: upload %s (%s Bytes)\n", uid, fname, fsize);
        strcpy(transactions[i]->fop, "U");

        strcpy(transactions[i]->fname, fname);
    } else {
        if (!strcmp(command, "LST")) {
            strcpy(type, "list");
            strcpy(transactions[i]->fop, "L");
        } else if (!strcmp(command, "REM")) {
            strcpy(type, "remove");
            strcpy(transactions[i]->fop, "X");
        }
        sprintf(buffer, "UID=%s: %s\n", uid, type);
    }

    printv(buffer);
    sprintf(message, "VLD %s %s", uid, tid);
    n = sendto(fd_udp, message, strlen(message), 0, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1) printError("validateOperation: sendto()");
}

void doOperation(char *buffer) {
    char uid[7], tid[6], fname[32], reply[128], command[5];
    char fop;
    int n, i, fd = 0;
    sscanf(buffer, "%s %s %s", command, uid, tid);
    if (!strcmp(command, "CNF")) {
        for (i = 0; i < numTransactions; i++) {
            if (!strcmp(tid, transactions[i]->tid))
                break;
        }
        if (i == numTransactions - 1) {
            strcpy(reply, "ERR\n");
            n = write(fd, reply, strlen(reply));
            if (n == -1) printError("doOperation: write()");
            return;
        }

        // TODO pass transactions[i] to all following functions instead of current parameters

        for (i = 0; i < numClients; i++) {
            if (!strcmp(users[i]->uid, uid)) {
                fd = users[i]->fd;
            }
        }

        switch (fop) {
        case 'L':
            strcpy(reply, list(uid, tid));
            break;
        case 'D':
            sscanf(buffer, "%s %s %c %s", uid, tid, &fop, fname);
            delete (fd, uid, fname);
            break;
        case 'R':
            sscanf(buffer, "%s %s %c %s", uid, tid, &fop, fname);
            retrieve(fd, uid, fname);
            break;
        case 'U':
            sscanf(buffer, "%s %s %c %s", uid, tid, &fop, fname);
            upload(fd, uid, fname);
            break;
        case 'X':
            strcpy(reply, removeAll(uid, tid));
            break;
        case 'E':
            strcpy(reply, "INV"); //TODO get acr
        }
        printv("operation validated");
    } else
        strcpy(reply, "ERR\n");
    n = write(fd, reply, strlen(reply));
    if (n == -1) printError("doOperation: write()");
}

/*      === main code ===        */

void fdManager() {
    char buffer[128];
    int i, n, maxfdp1, tcp_flag;

    while (1) {
        FD_ZERO(&rset);
        FD_SET(fd_udp, &rset);
        FD_SET(fd_tcp, &rset);

        for (i = 0; i < numClients; i++) {
            if (users[i]->fd != -5)
                FD_SET(users[i]->fd, &rset);
        }

        maxfdp1 = MAX(fd_tcp, fd_udp) + 1;

        for (i = 0; i < numClients; i++) {
            maxfdp1 = MAX(maxfdp1, users[i]->fd) + 1;
        }

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd_udp, &rset)) { // receive message from AS
            n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr *)&addr_udp, &addrlen_udp);
            if (n == -1) printError("main: recvfrom()");
            buffer[n] = '\0';
            printv("received message from as"); //DEBUG

            doOperation(buffer);
        }

        if (FD_ISSET(fd_tcp, &rset)) { // receive new connection from user
            printv("entrei");          // DEBUG

            for (i = 0; i < numClients + 1; i++) {
                if (users[i] == NULL) {
                    users[i] = (User)malloc(sizeof(struct user));
                    if ((users[i]->fd = accept(fd_tcp, (struct sockaddr *)&addr_tcp, &addrlen_tcp)) == -1)
                        printError("main: accept()");
                    break;
                }
            }
            if (i == numClients)
                users = (User *)realloc(users, sizeof(User) * (++numClients));
        }

        for (i = 0; i < numClients; i++) { // receive command from User
            if (FD_ISSET(users[i]->fd, &rset)) {
                userSession(i);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int i;

    if (argc < MINARGS || argc > MAXARGS) {
        printf("Usage: %s [-q FSport] [-n ASIP] [-p ASport] [-v]\n", argv[0]);
        printError("incorrect number of arguments");
    }

    strcpy(fsport, FSPORT);
    strcpy(asport, ASPORT);
    strcpy(asip, ASIP);

    for (i = MINARGS; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            verbose = TRUE; // TODO move to common file
        } else if (!strcmp(argv[i], "-h")) {
            printf("Usage: %s [-q FSport] [-n ASIP] [-p ASport] [-v]\n", argv[0]);
            exit(0);
        } else if (!strcmp(argv[i], "-q")) {
            strcpy(fsport, argv[++i]);
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        } else if (!strcmp(argv[i], "-n")) {
            strcpy(asip, argv[++i]);
        }
    }

    udpConnect(asip, asport, &fd_udp, &res_udp); // TODO sera que conectamos com o as logo aqui?

    printv("udp connection with AS established"); // DEBUG

    tcpOpenConnection(FSPORT, &fd_tcp, &res_tcp);
    if (listen(fd_tcp, 5) == -1) printError("TCP: listen()");
    users = (User *)malloc(sizeof(struct user));

    printv("tcp connection open"); // DEBUG

    signal(SIGINT, endFS);

    fdManager();

    return 0;
}
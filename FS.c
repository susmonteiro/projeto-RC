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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXARGS 8
#define MINARGS 1

#define MAX(a, b) a *(a > b) + b *(b >= a)

typedef struct user {
    int fd;
    char uid[7];
    int pending;
} * User;

typedef struct transaction {
    char uid[7];
    char tid[5];
    char vc[5];
    char fop[2];
    char fname[32];
    char fsize[32];
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

/*      === auxiliar functions ===       */

int readUntilSpace(int ind, char *buffer) { // TODO function common to other files?
    char c;
    //char path[64];
    int i = 0, n = 0, count = 0;
    do {
        n = read(users[ind]->fd, &c, 1);
        count += n;
        if (n == -1)
            printError("read()");
        else if (n == 0) {
            close(users[ind]->fd);

            users[ind] = NULL;
            return count;
        }
        buffer[i++] = c;
    } while (c != ' ' && c != '\0' && c != '\n');
    buffer[--i] = '\0';

    return count;
}

/*      === command functions ===        */

void sendNokReply(int fd, Transaction transaction) {
    int n;
    char reply[8];

    switch (transaction->fop[0]) {
        case 'R':
            strcpy(reply, "RRT NOK");
            break;
        case 'D':
            strcpy(reply, "RDL NOK");
            break;
        case 'X':
            strcpy(reply, "RRM NOK");
            break;
        default:
            strcpy(reply, "ERR");
    }
    n = write(fd, reply, strlen(reply));
    if (n == -1) printError("write()");
}

void listFiles(int fd, Transaction transaction) {
    DIR *d;
    FILE *file;
    char dirpath[32], message[270], files[400], reply[400], filepath[260];
    struct dirent *dir;
    int fsize, n_files = 0;
    sprintf(dirpath, "USERS/UID%s/FILES", transaction->uid);
    d = opendir(dirpath);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] != '.') {
                sprintf(filepath, "%s/%s", dirpath, dir->d_name);
                if (access(filepath, F_OK) != -1) {
                    printf("%s\n", dir->d_name);
                    file = fopen(filepath, "r");
                    fseek(file, 0, SEEK_END);
                    fsize = ftell(file);
                    fseek(file, 0, SEEK_SET);
                    sprintf(message, "%s %d ", dir->d_name, fsize);
                    if (n_files == 0)
                        strcpy(files, message);
                    else
                        strcat(files, message);
                    printf("%s\n", files);
                    n_files++;
                    fclose(file);
                }
            }
        }
        closedir(d);
    }
    if (n_files == 0) {
        write(fd, "RLS EOF\n", 8);
    } else {
        sprintf(message, "list operation successful for UID=%s", transaction->uid);
        printv(message);
        sprintf(reply, "RLS %d %s\n", n_files, files);
        printv(reply);
        write(fd, reply, strlen(reply));
    }
}

void deleteFile(int fd, Transaction transaction) {
    //TODO NOK, ERR
    char message[128], path[64];
    int n;

    sprintf(path, "USERS/UID%s/FILES/%s", transaction->uid, transaction->fname);
    if (remove(path) != 0) {
        n = write(fd, "RDL EOF\n", 8);
        if (n == -1) errorExit("write()");
        return;
    }

    sprintf(message, "file %s deleted for user %s", transaction->fname, transaction->uid);
    printv(message);
    n = write(fd, "RDL OK\n", 7);
    if (n == -1) errorExit("write()");
}

void retrieveFile(int fd, Transaction transaction) {
    //TODO RRT NOK, ERR
    int n, fsize;
    char message[128], buffer[128], path[64];
    FILE *file;

    sprintf(path, "USERS/UID%s/FILES/%s", transaction->uid, transaction->fname);
    if ((file = fopen(path, "r")) == NULL) {
        sprintf(message, "Error: file %s from user %s does not exist", transaction->fname, transaction->uid);
        printv(message);
        n = write(fd, "RRT EOF\n", 8);
        if (n == -1) errorExit("write()");
        return;
    }

    fseek(file, 0, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    sprintf(message, "file %s retrieved for user %s", transaction->fname, transaction->uid);
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

void uploadFile(int ind, Transaction transaction) {
    DIR *d;
    FILE *file;
    struct dirent *dir;
    char message[128];
    char data[128], dirpath[32], filepath[32], c;
    int n, i, charCount = 0, size, n_files = 0;

    sprintf(dirpath, "USERS/UID%s/FILES", transaction->uid);
    mkdir(dirpath, 0777);
    d = opendir(dirpath);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (!strcmp(dir->d_name, transaction->fname)) {
                n = write(users[ind]->fd, "RUP DUP\n", 8);
                if (n == -1) errorExit("write()");
                return;
            }
            n_files++;
        }
        closedir(d);
    }

    if (n_files == 15) {
        n = write(users[ind]->fd, "RUP FULL\n", 9);
        if (n == -1) errorExit("write()");
        return;
    }

    sprintf(filepath, "%s/%s", dirpath, transaction->fname);
    file = fopen(filepath, "w");

    sscanf(transaction->fsize, "%d", &size);

    for (i = 0; i < size; i++) {
        n = read(users[ind]->fd, &c, 1);
        if (n == -1)
            errorExit("read()");
        else if (n == 0) {
            printf("Error: FS closed\n");
            close(users[ind]->fd);
            users[ind] = NULL;
            return;
        }
        if (file != NULL)
            fputc(c, (FILE *)file);
    }
    if (file != NULL) fclose(file);

    sprintf(message, "%s stored for UID=%s", transaction->fname, transaction->uid);
    printv(message);
    n = write(users[ind]->fd, "RUP OK\n", 8);
    if (n == -1) errorExit("write()");
}

void removeAll(int fd, Transaction transaction) {
    char message[128], dirpath[64], filepath[64];
    int n;
    struct dirent *dir;
    DIR *d;

    sprintf(dirpath, "USERS/UID%s/FILES", transaction->uid);
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

    sprintf(message, "operation remove done for UID=%s", transaction->uid);
    printv(message);

    n = write(fd, "RRM OK\n", 7);
    if (n == -1) errorExit("write()");
}

/*      === user manager ===        */

void userSession(int ind) {
    int n, i;
    char buffer[128], message[128], command[5], uid[32], tid[32], fname[32], fsize[32], type[32];

    n = readUntilSpace(ind, command);

    if (users[ind] == NULL)
        return;

    readUntilSpace(ind, uid);
    readUntilSpace(ind, tid);

    strcpy(users[ind]->uid, uid);

    for (i = 0; i < numTransactions + 1; i++) {
        if (transactions[i] == NULL) {
            transactions[i] = (Transaction)malloc(sizeof(struct transaction));
            strcpy(transactions[i]->uid, uid);
            strcpy(transactions[i]->tid, tid);
            break;
        }
    }
    if (i == numTransactions) {
        numTransactions++;
        transactions = (Transaction *)realloc(transactions, sizeof(Transaction) * (numTransactions + 1));
        transactions[numTransactions] = NULL;
    }
    //sscanf(buffer, "%s %s %s", command, uid, tid);

    if (!strcmp(command, "RTV") || !strcmp(command, "DEL")) {
        if (!strcmp(command, "RTV")) {
            strcpy(type, "retrieve");
            strcpy(transactions[i]->fop, "R");

        } else if (!strcmp(command, "DEL")) {
            strcpy(type, "delete");
            strcpy(transactions[i]->fop, "D");
        }
        readUntilSpace(ind, fname);
        //sscanf(buffer, "%s %s %s %s", command, uid, tid, fname);
        sprintf(buffer, "UID=%s: %s %s", uid, type, fname);

        strcpy(transactions[i]->fname, fname);

    } else if (!strcmp(command, "UPL")) {
        readUntilSpace(ind, fname);
        readUntilSpace(ind, fsize);
        //sscanf(buffer, "%s %s %s %s %s", command, uid, tid, fname, fsize);
        sprintf(buffer, "UID=%s: upload %s (%s Bytes)", uid, fname, fsize);
        strcpy(transactions[i]->fop, "U");
        strcpy(transactions[i]->fname, fname);
        strcpy(transactions[i]->fsize, fsize);
    } else {
        if (!strcmp(command, "LST")) {
            strcpy(type, "list");
            strcpy(transactions[i]->fop, "L");
        } else if (!strcmp(command, "REM")) {
            strcpy(type, "remove");
            strcpy(transactions[i]->fop, "X");
        }
        sprintf(buffer, "UID=%s: %s", uid, type);
    }

    printv(buffer);
    sprintf(message, "VLD %s %s\n", uid, tid);
    n = sendto(fd_udp, message, strlen(message), 0, res_udp->ai_addr, res_udp->ai_addrlen);
    if (n == -1) printError("validateOperation: sendto()");

    users[ind]->pending = TRUE;
}

void sendInvReply(Transaction transaction) {
    /* int n;
    char reply[8];

    switch (transaction->fop) {
    case 'L':
        strcpy(reply, "RLS INV");
        break;
    case 'R':
        strcpy(reply, "RRT INV");
        break;
    case 'U':
        strcpy(reply, "RUP INV");
        break;
    case 'D':
        strcpy(reply, "RDL INV");
        break;
    default:
        strcpy(reply, "ERR");
    }
    n = write(fd, reply, strlen(reply));
    if (n == -1) printError("write()"); */
}

void doOperation(char *buffer) {
    char uid[7], tid[6], command[5];
    char *reply;
    int n, i, j, fd = 0;
    char fop;
    reply = (char *)malloc(sizeof(char) * 128);
    sscanf(buffer, "%s %s %s %c", command, uid, tid, &fop);
    if (!strcmp(command, "CNF")) {
        for (i = 0; i < numTransactions; i++) {
            if (!strcmp(tid, transactions[i]->tid))
                break;
        }
        if (i == numTransactions) {
            printv("Error: Transaction was not found");
            return;
        }

        for (j = 0; j < numClients; j++) {
            if (!strcmp(users[j]->uid, uid)) {
                fd = users[j]->fd;
                break;
            }
        }

        if (j == numClients) {
            printv("Error: User does not exist");
            for (j = 0; j < numClients; j++) {
                if (!strcmp(users[j]->uid, transactions[i]->uid)) {
                    sendNokReply(users[j]->fd, transactions[i]);
                    break;
                }
            }
        }

        switch (fop) {
            case 'L':
                listFiles(fd, transactions[i]);
                break;
            case 'D':
                deleteFile(fd, transactions[i]);
                break;
            case 'R':
                retrieveFile(fd, transactions[i]);
                break;
            case 'U':
                uploadFile(j, transactions[i]);
                break;
            case 'X':
                removeAll(fd, transactions[i]);
                break;
            case 'E':
                sendInvReply(transactions[i]);
        }
        printv("operation validated");
        transactions[i] = NULL;
    } else
        strcpy(reply, "ERR\n");
    n = write(fd, reply, strlen(reply));
    if (n == -1) printError("doOperation: write()");

    users[j]->pending = FALSE;
}

/*      === main code ===        */

void fdManager() {
    char buffer[128];
    int i, n, maxfdp1;

    while (1) {
        FD_ZERO(&rset);
        FD_SET(fd_udp, &rset);
        FD_SET(fd_tcp, &rset);
        for (i = 0; i < numClients; i++) {
            if (users[i] != NULL) {
                FD_SET(users[i]->fd, &rset);
            }
        }

        maxfdp1 = MAX(fd_tcp, fd_udp);

        for (i = 0; i < numClients; i++) {
            if (users[i] != NULL)
                maxfdp1 = MAX(maxfdp1, users[i]->fd);
        }

        maxfdp1++;

        select(maxfdp1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(fd_udp, &rset)) { // receive message from AS
            n = recvfrom(fd_udp, buffer, 128, 0, (struct sockaddr *)&addr_udp, &addrlen_udp);
            if (n == -1) printError("main: recvfrom()");
            buffer[n] = '\0';

            doOperation(buffer);
        }

        if (FD_ISSET(fd_tcp, &rset)) { // receive new connection from user
            for (i = 0; i < numClients + 1; i++) {
                if (users[i] == NULL) {
                    users[i] = (User)malloc(sizeof(struct user));
                    users[i]->pending = FALSE;
                    if ((users[i]->fd = accept(fd_tcp, (struct sockaddr *)&addr_tcp, &addrlen_tcp)) == -1)
                        printError("main: accept()");
                    break;
                }
            }
            if (i == numClients) {
                numClients++;
                users = (User *)realloc(users, sizeof(User) * (numClients + 1));
                users[numClients] = NULL;
            }
        }

        for (i = 0; i < numClients; i++) { // receive command from User
            if (users[i] != NULL && !users[i]->pending) {
                if (FD_ISSET(users[i]->fd, &rset)) {
                    userSession(i);
                }
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

    mkdir("USERS", 0777);

    udpConnect(asip, asport, &fd_udp, &res_udp); // TODO sera que conectamos com o as logo aqui?

    tcpOpenConnection(FSPORT, &fd_tcp, &res_tcp);
    if (listen(fd_tcp, 5) == -1) printError("TCP: listen()");

    users = (User *)malloc(sizeof(User));
    users[0] = NULL;

    transactions = (Transaction *)malloc(sizeof(Transaction));
    transactions[0] = NULL;

    signal(SIGINT, endFS);

    fdManager();

    return 0;
}

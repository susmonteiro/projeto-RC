/* User Application */

/* ===      TODO in User        ===
    - function retrieve file: it writes things it shouldn't to file 
*/

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
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAXARGS 10
#define MINARGS 1

#define CONNECTION_ON 1
#define CONNECTION_OFF 0

#define MAX(a, b) a *(a > b) + b *(b >= a)

int fsConnected = CONNECTION_OFF;

int fd_as, fd_fs;
fd_set rset;
struct addrinfo *res_as, *res_fs;

char fsip[32], fsport[8], asip[32], asport[8];
char uid[7], pass[10];

typedef struct request {
    char rid[5];
    char fop;
    char fname[32];
} * Request;

typedef struct transaction {
    char tid[5];
    char fop;
    char fname[32];
} * Transaction;

/*      === end User ===       */

void closeASconnection() {
    freeaddrinfo(res_as);
    close(fd_as);
}

void closeFSconnection() {
    if (fsConnected == CONNECTION_OFF) return;
    freeaddrinfo(res_fs);
    close(fd_fs);
    fsConnected = CONNECTION_OFF;
}

void closeConnections() {
    closeASconnection();
    closeFSconnection();
    exit(0);
}

/*      === auxiliar functions ===       */

void readUntilSpace(int fd, char *buffer) { // TODO function common to other files?
    char c;
    int i = 0, n = 0;
    do {
        n = read(fd, &c, 1);
        if (n == -1)
            errorExit("read()");
        else if (n == 0) {
            printf("Error: FS closed\n");
            closeConnections();
        }
        buffer[i++] = c;
    } while (c != ' ' && c != '\n');
    buffer[--i] = '\0';
}

/*      === command functions ===        */

void login() {
    // login UID pass
    int n;
    char message[64];
    scanf("%s %s", uid, pass);

    sprintf(message, "LOG %s %s\n", uid, pass);
    //printf("our message: %s", message);   //DEBUG
    n = write(fd_as, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void requestFile(Request req) {
    // req Fop [Fname]
    int n;
    char message[64];

    sprintf(req->rid, "%04d", rand() % 10000);

    scanf(" %c", &req->fop);
    switch (req->fop) {
    case 'L':
    case 'X':
        sprintf(message, "REQ %s %s %c\n", uid, req->rid, req->fop);
        break;
    case 'D':
    case 'R':
    case 'U':
        scanf("%s", req->fname);
        sprintf(message, "REQ %s %s %c %s\n", uid, req->rid, req->fop, req->fname);
        break;
    default:
        printf("Error: wrong command");
        return;
    }
    //printf("our message: %s\n", message); //DEBUG
    n = write(fd_as, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void validateCode(Request req) {
    // val VC
    int n;
    char message[64], vc[5];

    scanf("%s", vc);

    sprintf(message, "AUT %s %s %s\n", uid, req->rid, vc);
    //printf("%s\n", message); //DEBUG
    n = write(fd_as, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void listFiles(Transaction trans) {
    // list or l
    int n;
    char message[64];
    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;

    sprintf(message, "LST %s %s\n", uid, trans->tid);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void listFilesReply() {
    char status[64], filename[32], fsize[32];
    int i = 0, numFiles;

    readUntilSpace(fd_fs, status);

    if (!strcmp(status, "EOF")) {
        printf("Error: no files to list\n");
        return;
    } else if (!strcmp(status, "INV")) {
        printf("Error: tid is invalid\n");
        return;
    } else if (!strcmp(status, "ERR")) {
        printf("Error: bad request");
        return;
    } else {
        numFiles = atoi(status);
        if (numFiles == 0) {
            printf("Error: unexpected message from FS\n");
            closeConnections();
        }
        printf("numFiles=%d\n", numFiles); // DEBUG
    }

    for (i = 1; i <= numFiles; i++) {
        readUntilSpace(fd_fs, filename);
        readUntilSpace(fd_fs, fsize);
        printf("%d)\t%24s\t%10s Bytes\n", i, filename, fsize);
    }
}

void retrieveFile(Transaction trans) {
    //retrieve filename or r filename
    int n;
    char message[128];

    scanf("%s", trans->fname);

    // check if file exists
    if (access(trans->fname, F_OK) != -1) {
        printf("Error: file %s already exists\n", trans->fname);
        return;
    }

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;
    sprintf(message, "RTV %s %s %s\n", uid, trans->tid, trans->fname);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void retrieveFileReply(Transaction trans) {
    char c, status[64], fsize[32];
    int n, i, numChars;
    FILE *file;

    readUntilSpace(fd_fs, status);

    if (!strcmp(status, "OK")) {
        printf("Retrieve Sucessful\n");
    } else if (!strcmp(status, "EOF")) {
        printf("Error: file not available\n");
        return;
    } else if (!strcmp(status, "NOK")) {
        printf("Error: user %s has no content available\n", uid);
        return;
    } else if (!strcmp(status, "INV")) {
        printf("Error: tid is invalid\n");
        return;
    } else if (!strcmp(status, "ERR")) {
        printf("Error: bad request\n");
        return;
    } else {
        printf("Error: unexpected message from FS\n");
        closeConnections();
    }

    readUntilSpace(fd_fs, fsize);
    printf("File size: %s Bytes\n", fsize);
    numChars = atoi(fsize);

    if (access(trans->fname, F_OK) != -1) {
        printf("Error: file %s already exists\n", trans->fname);
        file = NULL;
    } else {
        file = fopen(trans->fname, "w");
    }

    for (i = 0; i < numChars; i++) {
        n = read(fd_fs, &c, 1);
        if (n == -1)
            errorExit("read()");
        else if (n == 0) {
            printf("Error: FS closed\n");
            closeConnections();
        }
        if (file != NULL)
            fputc(c, (FILE *)file);
    }
}

void uploadFile(Transaction trans) {
    // upload filename or u filename
    int n, fsize;
    char message[128], buffer[128];
    FILE *file;

    scanf("%s", trans->fname);

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;
    if ((file = fopen(trans->fname, "r")) == NULL) {
        printf("Error: file %s does not exist\n", trans->fname);
        closeFSconnection();
        return;
    }

    fseek(file, 0, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    sprintf(message, "UPL %s %s %s %d ", uid, trans->tid, trans->fname, fsize); // TODO
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");

    while (fgets(buffer, 128, (FILE *)file) != NULL) {
        n = write(fd_fs, buffer, strlen(buffer));
        if (n == -1) errorExit("write()");
    }
    fclose(file);
}

void uploadFileReply() {
    char status[64];

    readUntilSpace(fd_fs, status);

    if (!strcmp(status, "OK")) {
        printf("Upload Sucessful\n");
    } else if (!strcmp(status, "DUF")) {
        printf("Error: file already exists\n");
        return;
    } else if (!strcmp(status, "FULL")) {
        printf("Error: no more free space\n");
        return;
    } else if (!strcmp(status, "INV")) {
        printf("Error: tid is invalid\n");
        return;
    } else if (!strcmp(status, "ERR")) {
        printf("Error: bad request\n");
        return;
    } else {
        printf("Error: unexpected message from FS\n");
        closeConnections();
    }
}

void deleteFile(Transaction trans) {
    // delete filename or d filename
    int n;
    char message[128];

    scanf("%s", trans->fname);

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;
    sprintf(message, "DEL %s %s %s\n", uid, trans->tid, trans->fname);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void deleteFileReply() {
    char status[64];

    readUntilSpace(fd_fs, status);

    if (!strcmp(status, "OK")) {
        printf("Delete Sucessful\n");
    } else if (!strcmp(status, "EOF")) {
        printf("Error: file not available\n");
        return;
    } else if (!strcmp(status, "NOK")) {
        printf("Error: user %s does not exist\n", uid);
        return;
    } else if (!strcmp(status, "INV")) {
        printf("Error: tid is invalid\n");
        return;
    } else if (!strcmp(status, "ERR")) {
        printf("Error: bad request\n");
        return;
    } else {
        printf("Error: unexpected message from FS\n");
        closeConnections();
    }
}

void removeUser(Transaction trans) {
    // remove or x
    int n;
    char message[128];

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;
    sprintf(message, "REM %s %s\n", uid, trans->tid);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void removeUserReply() {
    char status[64];

    readUntilSpace(fd_fs, status);

    if (!strcmp(status, "OK")) {
        printf("Remove Sucessful\n");
    } else if (!strcmp(status, "NOK")) {
        printf("Error: user %s does not exist\n", uid);
        return;
    } else if (!strcmp(status, "INV")) {
        printf("Error: tid is invalid\n");
        return;
    } else if (!strcmp(status, "ERR")) {
        printf("Error: bad request\n");
        return;
    } else {
        printf("Error: unexpected message from FS\n");
        closeConnections();
    }
}

/*      === main code ===        */

void fdManager(Request req, Transaction trans) {
    char command[6], status[5];

    int n, maxfdp1;

    while (1) {
        FD_ZERO(&rset);
        FD_SET(STDIN, &rset);
        FD_SET(fd_as, &rset);
        if (fsConnected) {
            FD_SET(fd_fs, &rset);
        }

        maxfdp1 = MAX(STDIN, fd_as);
        if (fsConnected) maxfdp1 = MAX(maxfdp1, fd_fs);
        maxfdp1++;

        n = select(maxfdp1, &rset, NULL, NULL, NULL);
        if (n == -1) errorExit("select()");

        if (FD_ISSET(STDIN, &rset)) {
            scanf("%s", command);

            if (!strcmp(command, "login")) {
                login();
            } else if (!strcmp(command, "req")) {
                requestFile(req);
            } else if (!strcmp(command, "val")) {
                validateCode(req);
            } else {
                if ((!strcmp(command, "list")) || (!strcmp(command, "l"))) {
                    listFiles(trans);
                } else if ((!strcmp(command, "retrieve")) || (!strcmp(command, "r"))) {
                    retrieveFile(trans);
                } else if ((!strcmp(command, "upload")) || (!strcmp(command, "u"))) {
                    uploadFile(trans);
                } else if ((!strcmp(command, "delete")) || (!strcmp(command, "d"))) {
                    deleteFile(trans);
                } else if ((!strcmp(command, "remove")) || (!strcmp(command, "x"))) {
                    removeUser(trans);
                } else if (!strcmp(command, "exit")) {
                    closeConnections();
                } else {
                    printf("Error: invalid command\n");
                    continue;
                }
            }
        }

        if (FD_ISSET(fd_as, &rset)) {
            readUntilSpace(fd_as, command);
            readUntilSpace(fd_as, status);

            if (!strcmp(command, "RLO")) {
                if (!strcmp(status, "OK"))
                    printf("You are now logged in\n");
                else if (!strcmp(status, "NOK"))
                    printf("Error: login unsuccessful\n");
            } else if (!strcmp(command, "RRQ")) {
                if (!strcmp(status, "OK")) {
                    printf("Request successful\n");
                } else if (!strcmp(status, "ELOG"))
                    printf("Error: this user is not logged in\n");
                else if (!strcmp(status, "EPD"))
                    printf("Error: could not communicate with PD\n");
                else if (!strcmp(status, "EUSER"))
                    printf("Error: the UID is incorrect\n");
                else if (!strcmp(status, "EFOP"))
                    printf("Error: FOP is invalid");
            } else if (!strcmp(command, "RAU")) {
                if (!strcmp(status, "0"))
                    printf("Error: authentication unsuccessful");
                else {
                    printf("Authenticated  |   TID = %s\n", status);
                    strcpy(trans->tid, status);
                }
            } else if (!strcmp(command, "ERR")) {
                printf("Error: AS replied with ERR");
            } else {
                printf("Error: unexpected answer from AS\n");
                closeConnections();
            }
        }

        if (fsConnected == CONNECTION_OFF) continue;
        if (FD_ISSET(fd_fs, &rset)) {
            char typeMsg[4];
            n = read(fd_fs, typeMsg, 4); // read type + space
            if (n == -1)
                errorExit("read()");
            else if (n == 0) {
                printf("Error: FS closed\n");
                closeConnections();
            }
            typeMsg[3] = '\0';

            if (!strcmp(typeMsg, "RLS"))
                listFilesReply();
            else if (!strcmp(typeMsg, "RRT"))
                retrieveFileReply(trans);
            else if (!strcmp(typeMsg, "RUP"))
                uploadFileReply();
            else if (!strcmp(typeMsg, "RDL"))
                deleteFileReply();
            else if (!strcmp(typeMsg, "RRM"))
                removeUserReply();
            else {
                printf("Error: unexpected answer from FS\n");
                closeConnections();
            }

            closeFSconnection();
        }
    }
}

int main(int argc, char *argv[]) {
    int i;

    if (argc < MINARGS || argc > MAXARGS) {
        printf("​Usage: %s [-n ASIP] [-p ASport] [-m FSIP] [-q FSport]\n", argv[0]);
        errorExit("incorrect number of arguments");
    }

    strcpy(asip, ASIP);
    strcpy(asport, ASPORT);
    strcpy(fsip, FSIP);
    strcpy(fsport, FSPORT);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) {
            printf("​Usage: %s [-n ASIP] [-p ASport] [-m FSIP] [-q FSport]\n", argv[0]);
            exit(0);
        } else if (!strcmp(argv[i], "-n")) {
            strcpy(asip, argv[++i]);
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(asport, argv[++i]);
        } else if (!strcmp(argv[i], "-m")) {
            strcpy(fsip, argv[++i]);
        } else if (!strcmp(argv[i], "-q")) {
            strcpy(fsport, argv[++i]);
        }
    }

    srand(time(NULL));

    tcpConnect(asip, asport, &fd_as, &res_as);

    signal(SIGINT, closeConnections);
    signal(SIGPIPE, closeConnections);

    Request req = (Request)malloc(sizeof(struct request));
    strcpy(req->rid, "0000");
    req->fop = 'E';
    Transaction trans = (Transaction)malloc(sizeof(struct transaction));
    strcpy(trans->tid, "0000");
    trans->fop = 'E';

    fdManager(req, trans);

    return 0;
}
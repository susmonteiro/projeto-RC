/*  User Application 

invoked with: 
     ./user [-n ASIP] [-p ASport] [-m FSIP] [-q FSport]

Once the User application program is running, it establishes a TCP session with the AS,
which remains open, and then waits for the user to indicate the action to take.

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
#include <time.h>
#include <unistd.h>

#define MAXARGS 10
#define MINARGS 1

#define MAX(a, b) a *(a > b) + b *(b >= a)

void closeASconnection();
void closeFSconnection();
void closeConnections();

typedef struct request {
    char rid[5];
    char fop;
    int pending;
} * Request;

typedef struct transaction {
    char tid[5];
    char fop;
    char fname[32];
    int pending;
} * Transaction;

int fd_as, fd_fs;
fd_set rset;
struct addrinfo *res_as, *res_fs;

char fsip[32], fsport[8], asip[32], asport[8];
char uid[6], pass[9];
Request req;       // one request possible at a time
Transaction trans; // one transaction at a time

int endUser = FALSE;
int asConnected = FALSE, fsConnected = FALSE;

/*      === error functions ===       */

void errorExit(char *errorMessage) {
    if (errno != 0)
        printf("ERR: %s: %s\nExiting...\n", errorMessage, strerror(errno));
    else
        printf("ERR: %s\nExiting...\n", errorMessage);
    closeConnections();
}

/*      === sighandler functions ===       */

void exitHandler() {
    endUser = TRUE;
}

/*      === end User ===       */

void closeASconnection() {
    if (asConnected == FALSE) return;
    freeaddrinfo(res_as);
    close(fd_as);
    asConnected = FALSE;
}

void closeFSconnection() {
    if (fsConnected == FALSE) return;
    freeaddrinfo(res_fs);
    close(fd_fs);
    fsConnected = FALSE;
}

void closeConnections() {
    closeASconnection();
    closeFSconnection();
    free(req);
    free(trans);
    exit(0);
}

/*      === auxiliar functions ===       */

void readUntilSpace(int fd, char *buffer) {
    char c;
    int i = 0, n = 0;
    do {
        n = read(fd, &c, 1);
        if (n == -1)
            errorExit("read()");
        else if (n == 0) {
            printf("Error: server closed\n");
            closeConnections();
        }
        buffer[i++] = c;
    } while (c != ' ' && c != '\n');
    buffer[--i] = '\0';
}

void readGarbage() {
    char c;
    while ((c = getchar()) != '\n') {
    }
}

int isTransactionPending(Transaction trans) {
    if (trans->pending) {
        readGarbage();
        printf("Error: operation with FS pending\n");
        return TRUE;
    }
    return FALSE;
}

/*      === command functions ===        */

void login(char *tmpUid, char *tmpPass) {
    // login UID pass
    int n;
    char message[64];
    scanf("%s %s", tmpUid, tmpPass);

    if (strlen(tmpUid) != 5 || strlen(tmpPass) != 8) {
        printf("Error: invalid arguments\n");
        return;
    }

    if (asConnected == TRUE) {
        printf("Error: this user is already logged in\n");
        return;
    }

    sprintf(message, "LOG %s %s\n", tmpUid, tmpPass);
    n = write(fd_as, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void requestFile(Request req) {
    // req Fop [Fname]
    int n;
    char message[64], fname[32];

    if (req->pending) {
        readGarbage(); // ignore rest of request
        printf("Error: request pending\n");
        return;
    }

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
            scanf("%s", fname);
            sprintf(message, "REQ %s %s %c %s\n", uid, req->rid, req->fop, fname);
            break;
        default:
            printf("Error: wrong command\n");
            return;
    }
    n = write(fd_as, message, strlen(message));
    if (n == -1) errorExit("write()");

    req->pending = TRUE; // no more requests can be done until this one is validated
}

void validateCode(Request req) {
    // val VC
    int n;
    char message[64], vc[5];

    if (!req->pending) {
        readGarbage(); // ignore vc
        printf("Error: no request done\n");
        return;
    }

    scanf("%s", vc);

    sprintf(message, "AUT %s %s %s\n", uid, req->rid, vc);

    n = write(fd_as, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void listFiles(Transaction trans) {
    // list or l
    int n;
    char message[64];

    if (isTransactionPending(trans)) return;
    trans->fop = 'L';

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = TRUE;

    sprintf(message, "LST %s %s\n", uid, trans->tid);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
    trans->pending = TRUE;
}

void listFilesReply(Transaction trans) {
    char status[64], filename[32], fsize[32];
    int i = 0, numFiles;

    readUntilSpace(fd_fs, status);

    if (trans->fop != 'L') {
        printf("Error: operation not requested received\n");
        closeConnections();
    }

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
        printf("Number of Files: %d\n", numFiles);
    }

    for (i = 1; i <= numFiles; i++) {
        readUntilSpace(fd_fs, filename);
        readUntilSpace(fd_fs, fsize);
        printf("%d)\t%24s\t%10s Bytes\n", i, filename, fsize);
    }
    read(fd_fs, filename, 1);
}

void retrieveFile(Transaction trans) {
    // retrieve filename or r filename
    int n;
    char message[128];

    if (isTransactionPending(trans)) return;
    trans->fop = 'R';

    scanf("%s", trans->fname);

    // check if file exists
    if (access(trans->fname, F_OK) != -1) {
        printf("Error: file %s already exists\n", trans->fname);
        return;
    }

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = TRUE;
    sprintf(message, "RTV %s %s %s\n", uid, trans->tid, trans->fname);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
    trans->pending = TRUE;
}

void retrieveFileReply(Transaction trans) {
    char c, status[64], fsize[32];
    int n, i, numChars;
    FILE *file;

    readUntilSpace(fd_fs, status);

    if (trans->fop != 'R') {
        printf("Error: operation not requested received\n");
        closeConnections();
    }

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
    if (file != NULL) fclose(file);
}

void uploadFile(Transaction trans) {
    // upload filename or u filename
    int n, fsize, count;
    char buffer[128], message[128];
    FILE *file;

    if (isTransactionPending(trans)) return;
    trans->fop = 'U';

    scanf("%s", trans->fname);

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = TRUE;
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

    do {
        count = fread(buffer, 1, 128, (FILE *)file);
        n = write(fd_fs, buffer, count);
        if (n == -1) errorExit("write()");
    } while (count == 128);

    fclose(file);
    trans->pending = TRUE;
}

void uploadFileReply(Transaction trans) {
    char status[64];

    readUntilSpace(fd_fs, status);

    if (trans->fop != 'U') {
        printf("Error: operation not requested received\n");
        closeConnections();
    }

    if (!strcmp(status, "OK")) {
        printf("Upload Sucessful\n");
    } else if (!strcmp(status, "DUP")) {
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
        puts(status);
        printf("Error: unexpected message from FS\n");
        closeConnections();
    }
}

void deleteFile(Transaction trans) {
    // delete filename or d filename
    int n;
    char message[128];

    if (isTransactionPending(trans)) return;
    trans->fop = 'D';

    scanf("%s", trans->fname);

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = TRUE;
    sprintf(message, "DEL %s %s %s\n", uid, trans->tid, trans->fname);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
    trans->pending = TRUE;
}

void deleteFileReply(Transaction trans) {
    char status[64];

    readUntilSpace(fd_fs, status);

    if (trans->fop != 'D') {
        printf("Error: operation not requested received\n");
        closeConnections();
    }

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

    if (isTransactionPending(trans)) return;
    trans->fop = 'X';
    trans->pending = TRUE;

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = TRUE;
    sprintf(message, "REM %s %s\n", uid, trans->tid);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
    trans->pending = TRUE;
}

void removeUserReply(Transaction trans) {
    char status[64];

    readUntilSpace(fd_fs, status);

    if (trans->fop != 'X') {
        printf("Error: operation not requested received\n");
        closeConnections();
    }

    if (!strcmp(status, "OK")) {
        printf("Remove Sucessful\n");
        asConnected = FALSE;
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
    char command[6], status[5], tmpUid[7], tmpPass[10];

    int n, maxfdp1;

    while (1) {
        if (endUser) closeConnections();

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
        if (n == -1) continue; // if interrupted by signals

        if (FD_ISSET(STDIN, &rset)) { // receive command from terminal
            scanf("%s", command);

            if (strcmp(command, "login") && strcmp(command, "exit") && asConnected == FALSE) {
                readGarbage();
                printf("Error: first please login\n");
                continue;
            }

            if (!strcmp(command, "login")) {
                login(tmpUid, tmpPass);
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

        if (FD_ISSET(fd_as, &rset)) { // receive confirmation from AS
            readUntilSpace(fd_as, command);

            if (!strcmp(command, "ERR")) {
                errorExit("Error: AS replied with ERR");
            }

            readUntilSpace(fd_as, status);

            if (!strcmp(command, "RLO")) {
                if (!strcmp(status, "OK")) {
                    strcpy(uid, tmpUid);
                    strcpy(pass, tmpPass);
                    asConnected = TRUE;
                    printf("User %s is now logged in\n", uid);
                } else if (!strcmp(status, "NOK"))
                    printf("Error: login unsuccessful\n");
            } else if (!strcmp(command, "RRQ")) {
                if (!strcmp(status, "OK")) {
                    printf("Request successful\n");
                } else {
                    req->pending = FALSE;
                    if (!strcmp(status, "ELOG"))
                        printf("Error: this user is not logged in\n");
                    else if (!strcmp(status, "EPD"))
                        printf("Error: could not communicate with PD\n");
                    else if (!strcmp(status, "EUSER"))
                        printf("Error: the UID is incorrect\n");
                    else if (!strcmp(status, "EFOP"))
                        printf("Error: FOP is invalid");
                }
            } else if (!strcmp(command, "RAU")) {
                if (!strcmp(status, "0"))
                    printf("Error: authentication unsuccessful\n");
                else {
                    printf("Authenticated  |   TID = %s\n", status);
                    req->pending = FALSE;
                    strcpy(trans->tid, status);
                }
            } else {
                errorExit("Error: unexpected answer from AS\n");
            }
        }

        if (fsConnected == FALSE) continue; // check if user is connected to FS
        if (FD_ISSET(fd_fs, &rset)) {       // receive confirmation from FS
            char typeMsg[4];
            n = read(fd_fs, typeMsg, 4); // read type + space
            if (n == -1)
                errorExit("read()");
            else if (n == 0) {
                printf("Error: FS closed\n");
                closeConnections();
            }
            typeMsg[3] = '\0';

            if (trans->pending == FALSE) {
                printf("Error: operation not requested received\n");
                closeConnections();
            }
            trans->pending = FALSE;

            if (!strcmp(typeMsg, "RLS"))
                listFilesReply(trans);
            else if (!strcmp(typeMsg, "RRT"))
                retrieveFileReply(trans);
            else if (!strcmp(typeMsg, "RUP"))
                uploadFileReply(trans);
            else if (!strcmp(typeMsg, "RDL"))
                deleteFileReply(trans);
            else if (!strcmp(typeMsg, "RRM"))
                removeUserReply(trans);
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
        if (!strcmp(argv[i], "-h") || i + 1 >= argc) {
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

    srand(time(NULL)); // initialize rand()

    tcpConnect(asip, asport, &fd_as, &res_as);

    signal(SIGINT, exitHandler);

    req = (Request)malloc(sizeof(struct request));
    strcpy(req->rid, "0000");
    req->fop = 'E';
    req->pending = FALSE;

    trans = (Transaction)malloc(sizeof(struct transaction));
    strcpy(trans->tid, "0000");
    trans->fop = 'E';
    trans->pending = FALSE;

    fdManager(req, trans);

    return 0;
}
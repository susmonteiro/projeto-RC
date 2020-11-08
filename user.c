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
char uid[7], pass[10], vc[4], rid[5], file[32], tid[5];
char op;

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

void readUntilSpace(char *buffer) { // TODO function common to other files?
    char c;
    int i = 0, n = 0;
    do {
        n = read(fd_fs, &c, 1);
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
    sprintf(message, "LOG %s %s\n", uid, pass);
    //printf("our message: %s", message);   //DEBUG
    n = write(fd_as, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void requestFile() {
    // req Fop [Fname]
    int n;
    char message[64];

    scanf("%c", &op);
    switch (op) {
    case 'L':
        sprintf(message, "REQ %s %s %c\n", uid, rid, op);
        break;
    case 'D':
        scanf("%s", file);
        sprintf(message, "REQ %s %s %c %s\n", uid, rid, op, file);
        break;
    case 'R':
        scanf("%s", file);
        sprintf(message, "REQ %s %s %c %s\n", uid, rid, op, file);
        break;
    case 'U':
        scanf("%s", file);
        //printf("%c %s\n", op, file); //DEBUG
        //printf("%s %s %c %s\n", uid, rid, op, file); //DEBUG
        sprintf(message, "REQ %s %s %c %s\n", uid, rid, op, file);
        break;
    case 'X':
        sprintf(message, "REQ %s %s %c\n", uid, rid, op);
        break;
    default:
        printf("Error: wrong command");
        return;
    }
    //printf("our message: %s\n", message); //DEBUG
    n = write(fd_as, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void validateCode() {
    // val VC
    int n;
    char message[64];

    sprintf(message, "AUT %s %s %s\n", uid, rid, vc);
    //printf("%s\n", message); //DEBUG
    n = write(fd_as, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void listFiles() {
    // list or l
    int n;
    char message[64];
    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;

    sprintf(message, "LST %s %s\n", uid, tid);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void listFilesReply() {
    char status[64], filename[32], fsize[32];
    int i = 0, numFiles;

    readUntilSpace(status);
    printf("status read: %s\n", status); // DEBUG

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
        readUntilSpace(filename);
        readUntilSpace(fsize);
        printf("%d)\t%24s\t%10s Bytes\n", i, filename, fsize);
    }
}

void retrieveFile(char *filename) {
    //retrieve filename or r filename
    int n;
    char message[128];

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;
    sprintf(message, "RTV %s %s %s\n", uid, tid, filename);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void retrieveFileReply() {
    char status[64], buffer[128], fsize[32];
    int n;

    readUntilSpace(status);

    if (!strcmp(status, "OK")) {
        printf("Retrieve Sucessful\nPrinting Data...\n");
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

    readUntilSpace(fsize);
    printf("File size: %s Bytes\n", fsize);

    do {
        n = read(fd_fs, buffer, 128);
        if (n == -1)
            errorExit("read()");
        else if (n == 0) {
            printf("Error: FS closed\n");
            closeConnections();
        }
        printf("%s", buffer); // TODO strange char being printed
    } while (n == 128 && buffer[127] != '\n');
}

void uploadFile(char *filename) {
    // upload filename or u filename
    int n, fsize;
    char message[128], buffer[128];
    FILE *file;

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;
    if ((file = fopen(filename, "r")) == NULL) {
        printf("Error: file %s does not exist\n", filename);
        closeFSconnection();
        return;
    }

    fseek(file, 0, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    sprintf(message, "UPL %s %s %s %d ", uid, tid, filename, fsize); // TODO
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

    readUntilSpace(status);

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

void deleteFile(char *filename) {
    // delete filename or d filename
    int n;
    char message[128];

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;
    sprintf(message, "DEL %s %s %s\n", uid, tid, filename);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void deleteFileReply() {
    char status[64];

    readUntilSpace(status);

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

void removeUser() {
    // remove or x
    int n;
    char message[128];

    tcpConnect(fsip, fsport, &fd_fs, &res_fs);
    fsConnected = CONNECTION_ON;
    sprintf(message, "REM %s %s\n", uid, tid);
    n = write(fd_fs, message, strlen(message));
    if (n == -1) errorExit("write()");
}

void removeUserReply() {
    char status[64];

    readUntilSpace(status);

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

void fdManager() {
    char command[6], reply[128], acr[4], filename[32];

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
            printf("command: %s\n", command); // DEBUG
            if (!strcmp(command, "login")) {
                scanf("%s %s", uid, pass);
                login();
            } else if (!strcmp(command, "req")) {
                scanf("%c", &op);
                requestFile();
            } else if (!strcmp(command, "val")) {
                scanf("%s", vc);
                validateCode();
            } else if ((!strcmp(command, "list")) || (!strcmp(command, "l"))) {
                listFiles();
                printf("list sent\n"); // DEBUG
            } else if ((!strcmp(command, "retrieve")) || (!strcmp(command, "r"))) {
                scanf("%s", filename);
                retrieveFile(filename);
            } else if ((!strcmp(command, "upload")) || (!strcmp(command, "u"))) {
                scanf("%s", filename);
                uploadFile(filename);
            } else if ((!strcmp(command, "delete")) || (!strcmp(command, "d"))) {
                scanf("%s", filename);
                deleteFile(filename);
            } else if ((!strcmp(command, "remove")) || (!strcmp(command, "x"))) {
                removeUser();
            } else if (!strcmp(command, "exit")) {
                closeConnections();
            } else
                printf("Error: invalid command\n");
        }

        if (FD_ISSET(fd_as, &rset)) {
            n = read(fd_as, reply, 128);
            if (n == -1)
                errorExit("read()");
            else if (n == 0) {
                printf("Error: AS closed\n");
                closeConnections();
            }
            reply[n] = '\0';

            sscanf(reply, "%s %s", acr, tid);
            tid[4] = '\0';

            if (!strcmp(reply, "RLO OK\n"))
                printf("You are now logged in\n");
            else if (!strcmp(reply, "RLO NOK\n"))
                printf("Error: login unsuccessful\n");
            else if (!strcmp(reply, "RRQ OK\n"))
                printf("Request successful\n");
            else if (!strcmp(reply, "RRQ ELOG\n"))
                printf("Error: this user is not logged in\n");
            else if (!strcmp(reply, "RRQ EPD\n"))
                printf("Error: could not communicate with PD\n");
            else if (!strcmp(reply, "RRQ EUSER\n"))
                printf("Error: the UID is incorrect\n");
            else if (!strcmp(reply, "RRQ EFOP\n"))
                printf("Error: FOP is invalid");
            else if (!strcmp(acr, "RAU")) {
                if (!strcmp(tid, "0"))
                    printf("Error: authentication unsuccessful");
                else
                    printf("Authenticated  |   TID = %s\n", tid);
            } else {
                printf("Error: unexpected answer from AS\n");
                closeConnections();
            }
        }
        if (fsConnected == CONNECTION_OFF) continue;
        if (FD_ISSET(fd_fs, &rset)) {
            printf("inside fs select\n"); // DEBUG
            char typeMsg[4];
            n = read(fd_fs, typeMsg, 4); // read type + space
            if (n == -1)
                errorExit("read()");
            else if (n == 0) {
                printf("Error: FS closed\n");
                closeConnections();
            }
            typeMsg[3] = '\0';

            printf("received message: %s\n", typeMsg); // DEBUG

            if (!strcmp(typeMsg, "RLS"))
                listFilesReply();
            else if (!strcmp(typeMsg, "RRT"))
                retrieveFileReply();
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

    tcpConnect(asip, asport, &fd_as, &res_as);
    sprintf(rid, "%d", rand() % 9999);

    signal(SIGINT, closeConnections);
    signal(SIGPIPE, closeConnections);

    fdManager();

    return 0;
}
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "error.h"
#include "connection.h"

void udpOpenConnection(char *port, int *fd, struct addrinfo **res) {
    int errcode;
    ssize_t n;
    struct addrinfo hints;

    *fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*fd == -1) errorExit("udpOpenConnection: socket()");
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;          //IPv4
    hints.ai_socktype = SOCK_DGRAM;     //UDP socket
    hints.ai_flags=AI_PASSIVE;

    errcode = getaddrinfo(NULL, port, &hints, res);
    if (errcode != 0) errorExit("udpOpenConnection: getaddrinfo()");

    n = bind(*fd, (*res)->ai_addr, (*res)->ai_addrlen);
    if (n == -1) errorExit("udpOpenConnection: bind()");
}

void udpConnect(char *ip, char *port, int *fd, struct addrinfo **res) {
    int n;
    struct addrinfo hints;

    *fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*fd == -1) exit(1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET; // IPv4
    hints.ai_socktype=SOCK_DGRAM; // UDP socket

    n = getaddrinfo(ip, port, &hints, res);
    if (n != 0) errorExit("udpConnect: socket()");
}

void tcpOpenConnection(char *port, int *fd, struct addrinfo **res) {
    int errcode;
    ssize_t n;
    struct addrinfo hints;

    *fd = socket(AF_INET,SOCK_STREAM,0);
    if(*fd == -1) printError("tcpOpenConnection: socket()");

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(NULL, port, &hints, res);
    if(errcode != 0) printError("tcpOpenConnection: getaddrinfo()");

    n = bind(*fd, (*res)->ai_addr, (*res)->ai_addrlen);
    if(n == -1) printError("tcpOpenConnection: bind()");
}

void tcpConnect(char *ip, char *port, int *fd, struct addrinfo **res) {
    int n;
    struct addrinfo hints;

    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd == -1) errorExit("tcpConnect: socket()");

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET; // IPv4
    hints.ai_socktype=SOCK_STREAM; // TCP socket

    n = getaddrinfo(ip, port, &hints, res);
    if (n != 0) errorExit("tcpConnect: getaddrinfo()");

    printf("halo\n");

    n = connect(*fd, (*res)->ai_addr, (*res)->ai_addrlen);
    if (n == -1) errorExit("tcpConnect: connect()");
    printf("%d\n", n);
}

/* void udpSend(int fd, struct addrinfo *res) {
    int n;

    n = sendto(fd, "Hello!\n", 7, 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) exit(1);
}

void udpReceive(int fd) {
    char buffer[128];
    struct sockaddr_in addr;
    socklen_t addrlen;
    int n;

    addrlen = sizeof(addr);
    n = recvfrom(fd, buffer, 128, 0, (struct sockaddr*) &addr, &addrlen);
    if (n == -1) exit(1);
} */

void closeConnection(int fd, struct addrinfo *res) {
    freeaddrinfo(res);
    close(fd);
}
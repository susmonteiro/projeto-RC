#include "connection.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void errorKill(char *errorMessage) {
    if (errno != 0)
        printf("ERR: %s: %s\n", errorMessage, strerror(errno));
    else
        printf("ERR: %s\n", errorMessage);
}

void udpOpenConnection(char *port, int *fd, struct addrinfo **res) {
    int errcode;
    ssize_t n;
    struct addrinfo hints;

    *fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*fd == -1) errorKill("udpOpenConnection: socket()");
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      //IPv4
    hints.ai_socktype = SOCK_DGRAM; //UDP socket
    hints.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(NULL, port, &hints, res);
    if (errcode != 0) errorKill("udpOpenConnection: getaddrinfo()");

    n = bind(*fd, (*res)->ai_addr, (*res)->ai_addrlen);
    if (n == -1) errorKill("udpOpenConnection: bind()");
}

void udpConnect(char *ip, char *port, int *fd, struct addrinfo **res) {
    int n;
    struct addrinfo hints;

    *fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*fd == -1) exit(1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket

    n = getaddrinfo(ip, port, &hints, res);
    if (n != 0) errorKill("udpConnect: socket()");
}

void tcpOpenConnection(char *port, int *fd, struct addrinfo **res) {
    int errcode;
    ssize_t n;
    struct addrinfo hints;

    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd == -1) errorKill("tcpOpenConnection: socket()");

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(NULL, port, &hints, res);
    if (errcode != 0) errorKill("tcpOpenConnection: getaddrinfo()");

    int flag = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
        errorKill("tcpOpenConnection: setsockopt()");
    }

    n = bind(*fd, (*res)->ai_addr, (*res)->ai_addrlen);
    if (n == -1) errorKill("tcpOpenConnection: bind()");
}

void tcpConnect(char *ip, char *port, int *fd, struct addrinfo **res) {
    int n;
    struct addrinfo hints;

    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd == -1) errorKill("tcpConnect: socket()");

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP socket

    n = getaddrinfo(ip, port, &hints, res);
    if (n != 0) errorKill("tcpConnect: getaddrinfo()");

    n = connect(*fd, (*res)->ai_addr, (*res)->ai_addrlen);
    if (n == -1) errorKill("tcpConnect: connect()");
}
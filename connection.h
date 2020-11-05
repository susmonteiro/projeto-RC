#ifndef CONNECTION_H
#define CONNECTION_H

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

void udpOpenConnection(char *port, int *fd, struct addrinfo **res);
void udpConnect(char *ip, char *port, int *fd, struct addrinfo **res);
void tcpOpenConnection(char *port, int *fd, struct addrinfo **res);
void tcpConnect(char *ip, char *port, int *fd, struct addrinfo **res);
void closeConnection(int fd, struct addrinfo *res);

#endif

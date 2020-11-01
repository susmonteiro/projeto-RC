#ifndef CONNECTION_H
#define CONNECTION_H

void udpOpenConnection(char *port, int *fd, struct addrinfo **res);
void udpConnect(char *ip, char *port, int *fd, struct addrinfo **res);
void tcpOpenConnection(char *port, int *fd, struct addrinfo **res);
void tcpConnect(char *ip, char *port, int *fd, struct addrinfo **res);
void closeConnection(int fd, struct addrinfo *res);

#endif

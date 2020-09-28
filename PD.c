#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define PORT "58001"

int fd;
struct addrinfo hints, *res;

void udpConnect() {
    int n;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) exit(1);

    memset(&hints, 0, sizeof hints);
    hints.ai_family=AF_INET; // IPv4
    hints.ai_socktype=SOCK_DGRAM; // UDP socket

    n = getaddrinfo("localhost", PORT, &hints, &res);
    if (n != 0) exit(1);
}

void registration(char* uid, char* pass) {
    int n;
    char message[32], reply[8];
    socklen_t addrlen;
    struct sockaddr_in addr;

    if (strlen(uid) != 5 || strlen(pass) != 8) {
        printf("ERR\n");
        return;
    }

    sprintf(message, "REG %s %s\n", uid, pass);
    n = sendto(fd, message, strlen(message)*sizeof(char), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) exit(1);

    n = recvfrom(fd, reply, 8, 0, (struct sockaddr*) &addr, &addrlen);
    if (n == -1) exit(1);

    if (!strcmp(reply, "RRG OK"))
        printf("Registration successful\n");
}

void unregistration() {

}

int main() {
    char command[5], uid[6], pass[9];

    udpConnect();

    while(1) {
        scanf("%s %s %s", command, uid, pass);

        if (!strcmp(command, "reg")) {
            registration(uid, pass);
        } else if (!strcmp(command, "exit")) {
            unregistration();
        } else printf("ERR\n");
    }

    return 0;
}
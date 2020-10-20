#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

void errorExit(char* errorMessage) {
    printf("ERR: %s: %s\nExiting...\n", errorMessage, strerror(errno));
    exit(1);
}

void printError(char* errorMessage) {
    printf("ERR: %s: %s\nNot exiting...\n", errorMessage, strerror(errno));
}
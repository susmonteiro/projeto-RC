#include <stdio.h>
#include <string.h>
#include "config.h"
//#include "connection.h"
#include "error.h"

#define MAXARGS 4
#define MINARGS 1

int verbose = FALSE;

char fsport[8];

int main(int argc, char* argv[]) {
    int i;

    if (argc < MINARGS || argc > MAXARGS) {
        printf("Usage: %s [-p FSPORT] [-v]\n", argv[0]);
        printError("incorrect number of arguments\n");
    }
    strcpy(fsport, FSPORT);

    for (i = MINARGS; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            verbose = TRUE;             // TODO move to common file
        } else if (!strcmp(argv[i], "-p")) {
            strcpy(fsport, argv[++i]);
        }
    }
    return 0;
}
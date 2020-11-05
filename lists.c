#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    int i; // DEBUG - remove this and add wanted values
    struct Node *next;
} Message;

// DEBUG - add these 2 lines to specific file
Message *head = NULL;
Message *last = NULL;

// DEBUG - remove this
int n = 0;

void push() {
    Message *new = (Message *)malloc(sizeof(Message));
    new->i = n++;

    if (head == NULL) {
        head = new;
    } else {
        last->next = new;
    }
    last = new;
    last->next = NULL;
}

Message *pop() {
    Message *out = head;
    if (out->next == NULL) {
        head = NULL;
        last = NULL;
    } else {
        head = out->next;
    }

    return out;
}

// DEBUG - remove main
int main() {
    push();
    push();
    push();
    while (head != NULL) {
        printf("num: %d\n", pop()->i);
    }
    return 0;
}
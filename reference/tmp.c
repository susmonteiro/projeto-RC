#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void snooze() {
    printf("snooze\n");
    alarm(1);
}

int main() {

    signal(SIGALRM, snooze);
    alarm(1);

    while(1)
        sleep(0.5);
    return 0;
}
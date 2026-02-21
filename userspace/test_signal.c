#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

volatile int signal_received = 0;

void my_handler(int sig) {
    printf("[USER] Caught signal %d!\n", sig);
    signal_received = 1;
}

int main() {
    printf("[USER] Test Signal Program Started\n");

    /* Register Handler */
    printf("[USER] Registering handler for SIGUSR1 (10)...\n");
    if (signal(SIGUSR1, my_handler) == SIG_ERR) {
        printf("[USER] Failed to register handler\n");
        return 1;
    }

    /* Raise Signal */
    printf("[USER] Raising SIGUSR1...\n");
    if (raise(SIGUSR1) != 0) {
        printf("[USER] Failed to raise signal\n");
        return 1;
    }

    /* Check if handled */
    if (signal_received) {
        printf("[USER] SUCCESS: Signal handled correctly.\n");
        return 0;
    } else {
        printf("[USER] FAILURE: Signal handler was not executed.\n");
        return 1;
    }
}

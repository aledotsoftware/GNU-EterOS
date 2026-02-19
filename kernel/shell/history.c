#include "shell_internal.h"

#define HISTORY_SIZE    8
#define HISTORY_LEN     SHELL_MAX_INPUT

static char history[HISTORY_SIZE][HISTORY_LEN];
static unsigned int history_count = 0;

void shell_history_init(void) {
    history_count = 0;
}

void shell_history_add(const char* line) {
    if (!line || !*line) return;

    // Check duplicate of last entry
    if (history_count > 0) {
        if (strcmp(history[(history_count - 1) % HISTORY_SIZE], line) == 0) {
            return;
        }
    }

    strlcpy(history[history_count % HISTORY_SIZE], line, HISTORY_LEN);
    history_count++;
}

const char* shell_history_get(unsigned int index) {
    if (index >= history_count) return (void*)0;
    return history[index % HISTORY_SIZE];
}

unsigned int shell_history_count(void) {
    return history_count;
}

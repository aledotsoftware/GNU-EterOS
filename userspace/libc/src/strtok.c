#include <string.h>
#include <stddef.h>

char *strtok(char *str, const char *delim) {
    static char *next_token = NULL;
    char *token;

    if (str) next_token = str;
    if (!next_token) return NULL;

    /* Skip leading delimiters */
    while (*next_token) {
        if (!strchr(delim, *next_token)) break;
        next_token++;
    }

    if (!*next_token) {
        next_token = NULL;
        return NULL;
    }

    token = next_token;

    /* Find end of token */
    while (*next_token) {
        if (strchr(delim, *next_token)) {
            *next_token = '\0';
            next_token++;
            /* Store state for next call */
            return token;
        }
        next_token++;
    }

    /* End of string reached, next call returns NULL */
    next_token = NULL;
    return token;
}

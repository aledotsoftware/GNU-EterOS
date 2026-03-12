#include <string.h>
#include <stddef.h>

char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *token;

    if (str) *saveptr = str;
    if (!*saveptr) return NULL;

    /* Skip leading delimiters */
    while (**saveptr) {
        if (!strchr(delim, **saveptr)) break;
        (*saveptr)++;
    }

    if (!**saveptr) {
        *saveptr = NULL;
        return NULL;
    }

    token = *saveptr;

    /* Find end of token */
    while (**saveptr) {
        if (strchr(delim, **saveptr)) {
            **saveptr = '\0';
            (*saveptr)++;
            return token;
        }
        (*saveptr)++;
    }

    /* End of string reached, next call returns NULL */
    *saveptr = NULL;
    return token;
}

char *strtok(char *str, const char *delim) {
    static char *next_token = NULL;
    return strtok_r(str, delim, &next_token);
}

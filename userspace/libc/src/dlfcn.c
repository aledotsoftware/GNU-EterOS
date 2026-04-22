#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int used;
    char path[256];
    int flags;
} dl_handle_t;

static dl_handle_t g_handles[16];
static char g_dlerror[128];
static int g_dlerror_set = 0;

static void dl_set_error(const char *msg) {
    if (!msg) msg = "dlerror";
    strlcpy(g_dlerror, msg, sizeof(g_dlerror));
    g_dlerror_set = 1;
}

static void *dl_builtin_symbol(const char *symbol) {
    if (!symbol) return (void*)0;

    if (strcmp(symbol, "printf") == 0) return (void*)printf;
    if (strcmp(symbol, "malloc") == 0) return (void*)malloc;
    if (strcmp(symbol, "free") == 0) return (void*)free;
    if (strcmp(symbol, "exit") == 0) return (void*)exit;
    if (strcmp(symbol, "system") == 0) return (void*)system;

    if (strcmp(symbol, "dlopen") == 0) return (void*)dlopen;
    if (strcmp(symbol, "dlsym") == 0) return (void*)dlsym;
    if (strcmp(symbol, "dlclose") == 0) return (void*)dlclose;
    if (strcmp(symbol, "dlerror") == 0) return (void*)dlerror;

    return (void*)0;
}

void *dlopen(const char *filename, int flag) {
    if (!filename || filename[0] == '\0') {
        g_dlerror_set = 0;
        return RTLD_SELF;
    }

    {
        int fd = open(filename, O_RDONLY);
        if (fd < 0) {
            dl_set_error("dlopen: file not found");
            return (void*)0;
        }
        close(fd);
    }

    for (int i = 0; i < (int)(sizeof(g_handles) / sizeof(g_handles[0])); i++) {
        if (!g_handles[i].used) {
            g_handles[i].used = 1;
            g_handles[i].flags = flag;
            strlcpy(g_handles[i].path, filename, sizeof(g_handles[i].path));
            g_dlerror_set = 0;
            return (void*)&g_handles[i];
        }
    }

    dl_set_error("dlopen: no free handle slots");
    return (void*)0;
}

char *dlerror(void) {
    if (!g_dlerror_set) return (char*)0;
    g_dlerror_set = 0;
    return g_dlerror;
}

void *dlsym(void *handle, const char *symbol) {
    void *sym = (void*)0;

    if (handle == RTLD_DEFAULT || handle == RTLD_SELF) {
        sym = dl_builtin_symbol(symbol);
        if (sym) {
            g_dlerror_set = 0;
            return sym;
        }
        dl_set_error("dlsym: symbol not found");
        return (void*)0;
    }

    if (!handle) {
        dl_set_error("dlsym: invalid handle");
        return (void*)0;
    }

    {
        dl_handle_t *h = (dl_handle_t*)handle;
        if (!h->used) {
            dl_set_error("dlsym: closed handle");
            return (void*)0;
        }
    }

    sym = dl_builtin_symbol(symbol);
    if (sym) {
        g_dlerror_set = 0;
        return sym;
    }

    dl_set_error("dlsym: symbol not found");
    return (void*)0;
}

int dlclose(void *handle) {
    if (handle == RTLD_DEFAULT || handle == RTLD_SELF) {
        g_dlerror_set = 0;
        return 0;
    }

    if (!handle) {
        dl_set_error("dlclose: invalid handle");
        return -1;
    }

    {
        dl_handle_t *h = (dl_handle_t*)handle;
        if (!h->used) {
            dl_set_error("dlclose: already closed");
            return -1;
        }
        h->used = 0;
        h->flags = 0;
        h->path[0] = '\0';
    }

    g_dlerror_set = 0;
    return 0;
}

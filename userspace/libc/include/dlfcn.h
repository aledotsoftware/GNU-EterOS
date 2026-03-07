#ifndef _DLFCN_H
#define _DLFCN_H

#define RTLD_LAZY   1
#define RTLD_NOW    2
#define RTLD_GLOBAL 256
#define RTLD_LOCAL  0

static inline void *dlopen(const char *filename, int flag) { return (void*)0; }
static inline char *dlerror(void) { return "Dynamic linking not supported"; }
static inline void *dlsym(void *handle, const char *symbol) { return (void*)0; }
static inline int dlclose(void *handle) { return -1; }

#endif /* _DLFCN_H */

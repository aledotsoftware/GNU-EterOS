#include "shell_internal.h"
#include "../../include/task.h"
#include "../../include/syscall.h"
#include "../../include/string.h"
#include "../../include/mm.h"
#include "../../include/user_mode.h"
#include "../../include/serial.h"

/* Stash for passing data to new task safely */
static struct {
    char cmdline[256];
    volatile int busy;
} exec_stash;

static void exec_bootstrap(void) {
    /* 1. Parse cmdline from stash */
    /* We work directly on the stash buffer because we hold the lock (busy=1) */
    char* cmdline = (char*)exec_stash.cmdline;

    /* 2. Build argv */
    char* argv[16];
    int argc = 0;

    /* Tokenize in-place */
    char* token = cmdline;
    while (*token && argc < 15) {
        while (*token == ' ') token++; /* skip spaces */
        if (*token == 0) break;

        argv[argc++] = token;

        while (*token && *token != ' ') token++; /* find end of word */
        if (*token) {
            *token = 0; /* null terminate */
            token++;
        }
    }
    argv[argc] = NULL;

    if (argc == 0) {
        exec_stash.busy = 0;
        return;
    }

    const char* path = argv[0];

    /* Environment */
    char* envp[] = { "PATH=/bin", "HOME=/", "TERM=eteros", NULL };

    serial_write_string("[EXEC] Loading: ");
    serial_write_string(path);
    serial_write_string("\n");

    /* 3. Load ELF */
    uint64_t entry, rsp;
    int ret = execve_load(path, argv, envp, &entry, &rsp);

    /* Release stash now that we have loaded (copied) everything to the new user stack */
    exec_stash.busy = 0;

    if (ret != 0) {
        serial_write_string("[EXEC] Failed to load ELF (Error code).\n");
        /* Task exits automatically on return */
        return;
    }

    /* 4. Enter User Mode */
    serial_write_string("[EXEC] Jumping to User Mode...\n");
    enter_user_mode((void*)entry, (void*)rsp);
}

void cmd_exec(const char* args) {
    if (exec_stash.busy) {
        terminal_write_string("Error: Exec system is busy (previous command pending).\n");
        return;
    }

    if (!args || !*args) {
        terminal_write_string("Usage: exec <path> [args...]\n");
        return;
    }

    /* Copy args to stash */
    strlcpy((char*)exec_stash.cmdline, args, sizeof(exec_stash.cmdline));
    exec_stash.busy = 1;

    /* Spawn the loader task */
    int pid = task_create("user_loader", exec_bootstrap);
    if (pid < 0) {
        terminal_write_string("Error creating loader task.\n");
        exec_stash.busy = 0;
        return;
    }

    terminal_write_string("Process spawned (PID ");
    char buf[32];
    itoa_s(pid, buf, sizeof(buf), 10);
    terminal_write_string(buf);
    terminal_write_string(")\n");

    /* Wait for the loader to process the stash (copy to stack) */
    /* This prevents the shell from modifying exec_stash before the new task reads it */
    while (exec_stash.busy) {
        task_yield();
    }
}

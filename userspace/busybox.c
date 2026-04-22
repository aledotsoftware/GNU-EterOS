#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_PATH_LEN 512

static const char* bb_basename(const char* p) {
    const char* last = p;
    if (!p) return "";
    while (*p) {
        if (*p == '/') last = p + 1;
        p++;
    }
    return last;
}

static int bb_mkdir_p(const char* path) {
    char tmp[MAX_PATH_LEN];
    size_t len;
    size_t i;

    if (!path || !*path) return -1;
    len = strlen(path);
    if (len >= sizeof(tmp)) return -1;
    strcpy(tmp, path);

    for (i = 1; i < len; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (tmp[0] && mkdir(tmp, 0755) < 0 && errno != EEXIST) return -1;
            tmp[i] = '/';
        }
    }
    if (mkdir(tmp, 0755) < 0 && errno != EEXIST) return -1;
    return 0;
}

static int bb_rm_rf(const char* path) {
    struct stat st;
    DIR* d;
    struct dirent* ent;
    char child[MAX_PATH_LEN];

    if (lstat(path, &st) < 0) return -1;
    if (!S_ISDIR(st.st_mode)) return unlink(path);

    d = opendir(path);
    if (!d) return -1;

    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (snprintf(child, sizeof(child), "%s/%s", path, ent->d_name) >= (int)sizeof(child)) {
            closedir(d);
            errno = EINVAL;
            return -1;
        }
        if (bb_rm_rf(child) < 0) {
            closedir(d);
            return -1;
        }
    }

    closedir(d);
    return rmdir(path);
}

static int applet_echo(int argc, char** argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (i > 1) printf(" ");
        printf("%s", argv[i]);
    }
    printf("\n");
    return 0;
}

static int applet_true(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return 0;
}

static int applet_false(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return 1;
}

static int applet_uname(int argc, char** argv) {
    struct utsname u;
    (void)argc;
    (void)argv;
    if (uname(&u) < 0) return 1;
    printf("%s\n", u.sysname);
    return 0;
}

static int applet_pwd(int argc, char** argv) {
    char buf[256];
    (void)argc;
    (void)argv;
    if (!getcwd(buf, sizeof(buf))) return 1;
    printf("%s\n", buf);
    return 0;
}

static int applet_ls(int argc, char** argv) {
    const char* path = ".";
    int show_all = 0;
    DIR* d;
    struct dirent* ent;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) show_all = 1;
        else path = argv[i];
    }

    d = opendir(path);
    if (!d) {
        fprintf(stderr, "ls: cannot open %s\n", path);
        return 1;
    }

    while ((ent = readdir(d)) != NULL) {
        if (!show_all && ent->d_name[0] == '.') continue;
        printf("%s\n", ent->d_name);
    }
    closedir(d);
    return 0;
}

static int applet_cat(int argc, char** argv) {
    char buf[512];
    int i;
    if (argc < 2) return 1;
    for (i = 1; i < argc; i++) {
        FILE* f = fopen(argv[i], "r");
        if (!f) {
            fprintf(stderr, "cat: %s: %d\n", argv[i], errno);
            return 1;
        }
        while (fgets(buf, sizeof(buf), f)) {
            fputs(buf, stdout);
        }
        fclose(f);
    }
    return 0;
}

static int applet_sh(int argc, char** argv) {
    char* sh_argv[64];
    int outc = 0;

    sh_argv[outc++] = "sh";
    for (int i = 1; i < argc && outc < (int)(sizeof(sh_argv) / sizeof(sh_argv[0])) - 1; i++) {
        sh_argv[outc++] = argv[i];
    }
    sh_argv[outc] = NULL;

    execv("/sh.elf", sh_argv);
    fprintf(stderr, "sh: failed to exec /sh.elf: %d\n", errno);
    return 127;
}

static int applet_chmod(int argc, char** argv) {
    char* end = NULL;
    long mode;
    int rc = 0;

    if (argc < 3) {
        fprintf(stderr, "chmod: usage: chmod <mode> <file...>\n");
        return 1;
    }
    mode = strtol(argv[1], &end, 8);
    if (!end || *end != '\0' || mode < 0 || mode > 07777) {
        fprintf(stderr, "chmod: invalid mode: %s\n", argv[1]);
        return 1;
    }
    for (int i = 2; i < argc; i++) {
        if (chmod(argv[i], (mode_t)mode) < 0) {
            fprintf(stderr, "chmod: %s: %d\n", argv[i], errno);
            rc = 1;
        }
    }
    return rc;
}

static int applet_grep(int argc, char** argv) {
    const char* pattern;
    int rc = 1;
    int i;

    if (argc < 3) {
        fprintf(stderr, "grep: usage: grep <pattern> <file...>\n");
        return 1;
    }
    pattern = argv[1];

    for (i = 2; i < argc; i++) {
        FILE* f = fopen(argv[i], "r");
        char line[1024];
        if (!f) {
            fprintf(stderr, "grep: %s: %d\n", argv[i], errno);
            rc = 2;
            continue;
        }
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, pattern)) {
                fputs(line, stdout);
                rc = 0;
            }
        }
        fclose(f);
    }
    return rc;
}

static int applet_head(int argc, char** argv) {
    long lines = 10;
    int argi = 1;

    if (argc < 2) {
        fprintf(stderr, "head: usage: head [-n N] <file>\n");
        return 1;
    }
    if (argi < argc && strcmp(argv[argi], "-n") == 0) {
        char* end = NULL;
        if (argi + 2 >= argc) return 1;
        lines = strtol(argv[argi + 1], &end, 10);
        if (!end || *end != '\0' || lines < 0) return 1;
        argi += 2;
    }
    for (; argi < argc; argi++) {
        FILE* f = fopen(argv[argi], "r");
        char line[1024];
        long count = 0;
        if (!f) {
            fprintf(stderr, "head: %s: %d\n", argv[argi], errno);
            return 1;
        }
        while (count < lines && fgets(line, sizeof(line), f)) {
            fputs(line, stdout);
            count++;
        }
        fclose(f);
    }
    return 0;
}

static int applet_tail(int argc, char** argv) {
    long lines = 10;
    int argi = 1;
    char ring[128][1024];
    long total = 0;

    if (argc < 2) {
        fprintf(stderr, "tail: usage: tail [-n N] <file>\n");
        return 1;
    }
    if (argi < argc && strcmp(argv[argi], "-n") == 0) {
        char* end = NULL;
        if (argi + 2 >= argc) return 1;
        lines = strtol(argv[argi + 1], &end, 10);
        if (!end || *end != '\0' || lines <= 0 || lines > 128) return 1;
        argi += 2;
    }
    for (; argi < argc; argi++) {
        FILE* f = fopen(argv[argi], "r");
        char line[1024];
        if (!f) {
            fprintf(stderr, "tail: %s: %d\n", argv[argi], errno);
            return 1;
        }
        total = 0;
        while (fgets(line, sizeof(line), f)) {
            strncpy(ring[total % lines], line, sizeof(ring[0]) - 1);
            ring[total % lines][sizeof(ring[0]) - 1] = '\0';
            total++;
        }
        fclose(f);
        {
            long start = (total > lines) ? (total - lines) : 0;
            for (long i = start; i < total; i++) {
                fputs(ring[i % lines], stdout);
            }
        }
    }
    return 0;
}

static int applet_wc(int argc, char** argv) {
    int show_lines = 0;
    int show_words = 0;
    int show_bytes = 0;
    int argi = 1;

    if (argc < 2) {
        fprintf(stderr, "wc: usage: wc [-lwc] <file...>\n");
        return 1;
    }
    if (argi < argc && argv[argi][0] == '-') {
        const char* p = argv[argi] + 1;
        while (*p) {
            if (*p == 'l') show_lines = 1;
            else if (*p == 'w') show_words = 1;
            else if (*p == 'c') show_bytes = 1;
            else return 1;
            p++;
        }
        argi++;
    }
    if (!show_lines && !show_words && !show_bytes) {
        show_lines = show_words = show_bytes = 1;
    }
    for (; argi < argc; argi++) {
        FILE* f = fopen(argv[argi], "r");
        int ch;
        long lines = 0;
        long words = 0;
        long bytes = 0;
        int in_word = 0;
        if (!f) {
            fprintf(stderr, "wc: %s: %d\n", argv[argi], errno);
            return 1;
        }
        while ((ch = fgetc(f)) != EOF) {
            bytes++;
            if (ch == '\n') lines++;
            if (isspace(ch)) {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
        fclose(f);
        if (show_lines) printf("%ld ", lines);
        if (show_words) printf("%ld ", words);
        if (show_bytes) printf("%ld ", bytes);
        printf("%s\n", argv[argi]);
    }
    return 0;
}

static int applet_mkdir(int argc, char** argv) {
    int i;
    int pflag = 0;
    int rc = 0;
    if (argc < 2) return 1;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            pflag = 1;
            continue;
        }
        if (pflag) {
            if (bb_mkdir_p(argv[i]) < 0) {
                fprintf(stderr, "mkdir: %s: %d\n", argv[i], errno);
                rc = 1;
            }
        } else if (mkdir(argv[i], 0755) < 0) {
            fprintf(stderr, "mkdir: %s: %d\n", argv[i], errno);
            rc = 1;
        }
    }
    return rc;
}

static int applet_rm(int argc, char** argv) {
    int i;
    int rflag = 0;
    int fflag = 0;
    int rc = 0;

    if (argc < 2) return 1;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-rf") == 0 || strcmp(argv[i], "-fr") == 0) {
            rflag = 1;
            if (strcmp(argv[i], "-rf") == 0 || strcmp(argv[i], "-fr") == 0) fflag = 1;
            continue;
        }
        if (strcmp(argv[i], "-f") == 0) {
            fflag = 1;
            continue;
        }

        if (rflag) {
            if (bb_rm_rf(argv[i]) < 0 && !fflag) {
                fprintf(stderr, "rm: %s: %d\n", argv[i], errno);
                rc = 1;
            }
        } else if (unlink(argv[i]) < 0 && !fflag) {
            fprintf(stderr, "rm: %s: %d\n", argv[i], errno);
            rc = 1;
        }
    }
    return rc;
}

static int applet_touch(int argc, char** argv) {
    int i;
    if (argc < 2) return 1;
    for (i = 1; i < argc; i++) {
        int fd = open(argv[i], O_CREAT | O_WRONLY, 0644);
        if (fd < 0) {
            fprintf(stderr, "touch: %s: %d\n", argv[i], errno);
            return 1;
        }
        close(fd);
    }
    return 0;
}

static int applet_cp(int argc, char** argv) {
    FILE* in;
    FILE* out;
    char buf[1024];
    size_t n;

    if (argc != 3) return 1;
    in = fopen(argv[1], "r");
    if (!in) return 1;
    out = fopen(argv[2], "w");
    if (!out) {
        fclose(in);
        return 1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return 1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static int applet_mv(int argc, char** argv) {
    if (argc != 3) return 1;
    if (rename(argv[1], argv[2]) < 0) {
        if (applet_cp(argc, argv) != 0) return 1;
        if (unlink(argv[1]) < 0) return 1;
    }
    return 0;
}

static int applet_help(void) {
    printf("busybox applets:\n");
    printf("  ls cat echo pwd mkdir rm cp mv touch uname true false sh\n");
    printf("  grep head tail wc chmod\n");
    return 0;
}

int main(int argc, char** argv) {
    const char* applet;

    if (argc <= 0 || !argv || !argv[0]) return 1;
    applet = bb_basename(argv[0]);

    if (strcmp(applet, "busybox") == 0) {
        if (argc == 1 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "--list") == 0) {
            return applet_help();
        }
        applet = argv[1];
        argv++;
        argc--;
    }

    if (strcmp(applet, "ls") == 0) return applet_ls(argc, argv);
    if (strcmp(applet, "cat") == 0) return applet_cat(argc, argv);
    if (strcmp(applet, "echo") == 0) return applet_echo(argc, argv);
    if (strcmp(applet, "pwd") == 0) return applet_pwd(argc, argv);
    if (strcmp(applet, "mkdir") == 0) return applet_mkdir(argc, argv);
    if (strcmp(applet, "rm") == 0) return applet_rm(argc, argv);
    if (strcmp(applet, "cp") == 0) return applet_cp(argc, argv);
    if (strcmp(applet, "mv") == 0) return applet_mv(argc, argv);
    if (strcmp(applet, "touch") == 0) return applet_touch(argc, argv);
    if (strcmp(applet, "uname") == 0) return applet_uname(argc, argv);
    if (strcmp(applet, "true") == 0) return applet_true(argc, argv);
    if (strcmp(applet, "false") == 0) return applet_false(argc, argv);
    if (strcmp(applet, "sh") == 0) return applet_sh(argc, argv);
    if (strcmp(applet, "grep") == 0) return applet_grep(argc, argv);
    if (strcmp(applet, "head") == 0) return applet_head(argc, argv);
    if (strcmp(applet, "tail") == 0) return applet_tail(argc, argv);
    if (strcmp(applet, "wc") == 0) return applet_wc(argc, argv);
    if (strcmp(applet, "chmod") == 0) return applet_chmod(argc, argv);
    fprintf(stderr, "busybox: applet not found: %s\n", applet);
    return 1;
}

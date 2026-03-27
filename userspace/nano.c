#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NANO_BUFFER_SIZE 65536
#define NANO_LINE_SIZE 512

static char g_buffer[NANO_BUFFER_SIZE];
static size_t g_length = 0;

static int read_line_stdin(char *buf, int max_len) {
    int i = 0;
    char c;

    while (i < max_len - 1) {
        int r = read(STDIN_FILENO, &c, 1);
        if (r <= 0) {
            break;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            break;
        }
        buf[i++] = c;
    }

    buf[i] = '\0';
    return i;
}

static void print_help(const char *path) {
    printf("\nGNU nano 0.1 (modo simple para eterOS)\n");
    printf("Archivo: %s\n", path);
    printf("Comandos especiales:\n");
    printf("  /help   muestra esta ayuda\n");
    printf("  /save   guarda el archivo\n");
    printf("  /clear  limpia el buffer\n");
    printf("  /quit   sale del editor\n");
    printf("Cualquier otra linea se agrega al final del archivo.\n\n");
}

static void show_current_buffer(void) {
    if (g_length == 0) {
        printf("[archivo vacio]\n");
        return;
    }

    write(STDOUT_FILENO, g_buffer, g_length);
    if (g_buffer[g_length - 1] != '\n') {
        printf("\n");
    }
}

static int load_file(const char *path) {
    FILE *fp = fopen(path, "rb");

    if (!fp) {
        g_length = 0;
        g_buffer[0] = '\0';
        return 0;
    }

    g_length = fread(g_buffer, 1, sizeof(g_buffer) - 1, fp);
    g_buffer[g_length] = '\0';
    fclose(fp);
    return 0;
}

static int save_file(const char *path) {
    FILE *fp = fopen(path, "wb");

    if (!fp) {
        printf("nano: no se pudo guardar %s\n", path);
        return 1;
    }

    if (g_length > 0 && fwrite(g_buffer, 1, g_length, fp) != g_length) {
        fclose(fp);
        printf("nano: error escribiendo %s\n", path);
        return 1;
    }

    fclose(fp);
    printf("[guardado] %s (%d bytes)\n", path, (int)g_length);
    return 0;
}

static int append_line(const char *line) {
    size_t len = strlen(line);

    if (g_length + len + 1 >= sizeof(g_buffer)) {
        printf("nano: buffer lleno, no se puede agregar mas texto\n");
        return 1;
    }

    memcpy(g_buffer + g_length, line, len);
    g_length += len;
    g_buffer[g_length++] = '\n';
    g_buffer[g_length] = '\0';
    return 0;
}

int main(int argc, char *argv[]) {
    char line[NANO_LINE_SIZE];
    const char *path;

    if (argc < 2) {
        printf("Uso: nano <archivo>\n");
        return 1;
    }

    path = argv[1];
    load_file(path);

    print_help(path);
    show_current_buffer();

    while (1) {
        printf("nano> ");
        fflush(stdout);

        if (read_line_stdin(line, sizeof(line)) < 0) {
            break;
        }

        if (strcmp(line, "/help") == 0) {
            print_help(path);
            continue;
        }
        if (strcmp(line, "/save") == 0) {
            save_file(path);
            continue;
        }
        if (strcmp(line, "/clear") == 0) {
            g_length = 0;
            g_buffer[0] = '\0';
            printf("[buffer limpiado]\n");
            continue;
        }
        if (strcmp(line, "/quit") == 0) {
            printf("Saliendo de nano.\n");
            break;
        }

        append_line(line);
    }

    return 0;
}

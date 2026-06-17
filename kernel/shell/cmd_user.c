#include "shell_internal.h"
#include "../../include/fs/vfs.h"
#include "../../include/crypto/sha256.h"
#include "../../include/mm.h"
#include "../../include/string.h"

#define MAX_LINE 256

static int read_line_vfs(fs_node_t* node, uint32_t offset, char *buf, int max_len) {
    int i = 0;
    char c;
    while (i < max_len - 1) {
        int r = read_fs(node, offset + i, 1, (uint8_t*)&c);
        if (r <= 0) break;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return i;
}

static void cmd_useradd(const char* args) {
    char username[32] = {0};
    char password[32] = {0};

    const char* ptr = args;
    int idx = 0;
    while (*ptr && *ptr != ' ' && idx < 31) {
        username[idx++] = *ptr++;
    }
    if (*ptr == ' ') {
        ptr++;
        idx = 0;
        while (*ptr && idx < 31) {
            password[idx++] = *ptr++;
        }
    }

    if (username[0] == '\0' || password[0] == '\0') {
        terminal_write_string("  Uso: user add <usuario> <password>\n");
        return;
    }

    /* SHA256 */
    uint8_t hash[SHA256_BLOCK_SIZE];
    sha256((const uint8_t*)password, strlen(password), hash);
    char hash_str[SHA256_BLOCK_SIZE * 2 + 1];
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        char hex[3];
        utoa_hex_s(hash[i], hex, sizeof(hex));
        if (hash[i] < 16) {
            hash_str[i*2] = '0';
            hash_str[i*2+1] = hex[0];
        } else {
            hash_str[i*2] = hex[0];
            hash_str[i*2+1] = hex[1];
        }
    }
    hash_str[SHA256_BLOCK_SIZE * 2] = '\0';

    int next_uid = 1000;
    fs_node_t* passwd_node = vfs_lookup(fs_root, "/etc/passwd");
    if (passwd_node) {
        char line[MAX_LINE];
        uint32_t offset = 0;
        while (1) {
            int bytes = read_line_vfs(passwd_node, offset, line, sizeof(line));
            if (bytes <= 0) break;
            offset += bytes;

            char user[32] = {0};
            char uid_str[16] = {0};
            int field = 0, c_idx = 0, u_idx = 0;

            for (int i = 0; line[i] != '\0'; i++) {
                if (line[i] == ':') { field++; continue; }
                if (field == 0 && c_idx < 31) user[c_idx++] = line[i];
                else if (field == 2 && u_idx < 15) uid_str[u_idx++] = line[i];
            }

            if (strcmp(user, username) == 0) {
                terminal_write_string("  Error: El usuario ya existe.\n");
                return;
            }

            int uid = 0;
            atoi_s(uid_str, &uid);
            if (uid >= next_uid) next_uid = uid + 1;
        }
    } else {
        /* Create /etc/ if not exists */
        fs_node_t* etc_node = vfs_lookup(fs_root, "/etc");
        if (!etc_node) {
            vfs_mkdir("/etc", 0755);
            etc_node = vfs_lookup(fs_root, "/etc");
        }
        if (etc_node) create_fs(etc_node, "passwd", 0644);
        passwd_node = vfs_lookup(fs_root, "/etc/passwd");
        if (!passwd_node) {
            terminal_write_string("  Error: No se pudo crear /etc/passwd\n");
            return;
        }
    }

    fs_node_t* shadow_node = vfs_lookup(fs_root, "/etc/shadow");
    if (!shadow_node) {
        fs_node_t* etc_node = vfs_lookup(fs_root, "/etc");
        if (etc_node) create_fs(etc_node, "shadow", 0600);
        shadow_node = vfs_lookup(fs_root, "/etc/shadow");
        if (!shadow_node) {
            terminal_write_string("  Error: No se pudo crear /etc/shadow\n");
            return;
        }
    }
    if (shadow_node) shadow_node->mask = 0600;

    char passwd_entry[MAX_LINE];
    char uid_str[16];
    itoa_s(next_uid, uid_str, sizeof(uid_str), 10);

    int len = 0;
    // username:x:uid:gid::/home/username:/bin/sh
    for (int i = 0; username[i] != '\0' && len < MAX_LINE - 1; i++) passwd_entry[len++] = username[i];
    if (len < MAX_LINE - 1) passwd_entry[len++] = ':';
    passwd_entry[len++] = 'x';
    passwd_entry[len++] = ':';
    for (int i = 0; uid_str[i] != '\0' && len < MAX_LINE - 1; i++) passwd_entry[len++] = uid_str[i];
    if (len < MAX_LINE - 1) passwd_entry[len++] = ':';
    for (int i = 0; uid_str[i] != '\0' && len < MAX_LINE - 1; i++) passwd_entry[len++] = uid_str[i];
    if (len < MAX_LINE - 1) passwd_entry[len++] = ':';
    if (len < MAX_LINE - 1) passwd_entry[len++] = ':';
    passwd_entry[len++] = '/'; passwd_entry[len++] = 'h'; passwd_entry[len++] = 'o'; passwd_entry[len++] = 'm'; passwd_entry[len++] = 'e'; passwd_entry[len++] = '/';
    for (int i = 0; username[i] != '\0' && len < MAX_LINE - 1; i++) passwd_entry[len++] = username[i];
    if (len < MAX_LINE - 1) passwd_entry[len++] = ':';
    passwd_entry[len++] = '/'; passwd_entry[len++] = 'b'; passwd_entry[len++] = 'i'; passwd_entry[len++] = 'n'; passwd_entry[len++] = '/'; passwd_entry[len++] = 's'; passwd_entry[len++] = 'h';
    if (len < MAX_LINE - 1) passwd_entry[len++] = '\n';
    passwd_entry[len] = '\0';
    write_fs(passwd_node, passwd_node->length, strlen(passwd_entry), (uint8_t*)passwd_entry);


    char shadow_entry[MAX_LINE];
    len = 0;
    // username:hash:19000:0:99999:7:::
    for (int i = 0; username[i] != '\0' && len < MAX_LINE - 1; i++) shadow_entry[len++] = username[i];
    if (len < MAX_LINE - 1) shadow_entry[len++] = ':';
    for (int i = 0; hash_str[i] != '\0' && len < MAX_LINE - 1; i++) shadow_entry[len++] = hash_str[i];
    if (len < MAX_LINE - 1) shadow_entry[len++] = ':';
    shadow_entry[len++] = '1'; shadow_entry[len++] = '9'; shadow_entry[len++] = '0'; shadow_entry[len++] = '0'; shadow_entry[len++] = '0';
    if (len < MAX_LINE - 1) shadow_entry[len++] = ':';
    shadow_entry[len++] = '0';
    if (len < MAX_LINE - 1) shadow_entry[len++] = ':';
    shadow_entry[len++] = '9'; shadow_entry[len++] = '9'; shadow_entry[len++] = '9'; shadow_entry[len++] = '9'; shadow_entry[len++] = '9';
    if (len < MAX_LINE - 1) shadow_entry[len++] = ':';
    shadow_entry[len++] = '7';
    if (len < MAX_LINE - 1) shadow_entry[len++] = ':';
    if (len < MAX_LINE - 1) shadow_entry[len++] = ':';
    if (len < MAX_LINE - 1) shadow_entry[len++] = ':';
    if (len < MAX_LINE - 1) shadow_entry[len++] = '\n';
    shadow_entry[len] = '\0';
    write_fs(shadow_node, shadow_node->length, strlen(shadow_entry), (uint8_t*)shadow_entry);


    terminal_write_string("  Usuario creado exitosamente con UID ");
    terminal_write_string(uid_str);
    terminal_write_string(".\n");
}

static void remove_user_from_vfs_file(const char* filepath, const char* username) {
    fs_node_t* node = vfs_lookup(fs_root, filepath);
    if (!node) return;

    uint8_t* buffer = kmalloc(node->length + 1);
    if (!buffer) return;
    read_fs(node, 0, node->length, buffer);
    buffer[node->length] = '\0';

    fs_node_t* etc_node = vfs_lookup(fs_root, "/etc");
    char temp_name[32];
    int len = 0;
    for(int i = 5; filepath[i] != '\0' && len < 27; i++) {
        temp_name[len++] = filepath[i];
    }
    temp_name[len++] = '.'; temp_name[len++] = 't'; temp_name[len++] = 'm'; temp_name[len++] = 'p'; temp_name[len] = '\0';

    int is_shadow = 0;
    if (filepath[5] == 's' && filepath[6] == 'h') is_shadow = 1;

    create_fs(etc_node, temp_name, is_shadow ? 0600 : 0644);

    char temp_path[64];
    len = 0;
    temp_path[len++] = '/'; temp_path[len++] = 'e'; temp_path[len++] = 't'; temp_path[len++] = 'c'; temp_path[len++] = '/';
    for(int i = 0; temp_name[i] != '\0'; i++) temp_path[len++] = temp_name[i];
    temp_path[len] = '\0';

    fs_node_t* tmp_node = vfs_lookup(fs_root, temp_path);
    if (!tmp_node) {
        kfree(buffer);
        return;
    }
    if (tmp_node->truncate) tmp_node->truncate(tmp_node, 0);
    else tmp_node->length = 0;
    tmp_node->mask = is_shadow ? 0600 : 0644;

    char* ptr = (char*)buffer;
    int found = 0;
    while (*ptr) {
        char line[MAX_LINE] = {0};
        int l = 0;
        while (*ptr && *ptr != '\n' && l < MAX_LINE - 1) {
            line[l++] = *ptr++;
        }
        if (*ptr == '\n') {
            line[l++] = *ptr++;
        }
        line[l] = '\0';

        char user[32] = {0};
        int c_idx = 0;
        for (int i = 0; line[i] != '\0' && line[i] != ':'; i++) {
            if (c_idx < 31) user[c_idx++] = line[i];
        }

        if (strcmp(user, username) != 0) {
            write_fs(tmp_node, tmp_node->length, l, (uint8_t*)line);
        } else {
            found = 1;
        }
    }
    kfree(buffer);

    if (found) {
        uint8_t* new_buf = kmalloc(tmp_node->length);
        read_fs(tmp_node, 0, tmp_node->length, new_buf);
        if (node->truncate) node->truncate(node, 0);
        else node->length = 0; /* Fallback */
        write_fs(node, 0, tmp_node->length, new_buf);
        kfree(new_buf);
    }
}


static void cmd_userdel(const char* args) {
    if (!args || *args == '\0') {
        terminal_write_string("  Uso: user del <usuario>\n");
        return;
    }
    if (strcmp(args, "root") == 0) {
        terminal_write_string("  Error: No se puede eliminar al usuario root.\n");
        return;
    }

    fs_node_t* shadow_node = vfs_lookup(fs_root, "/etc/shadow");
    if (!shadow_node) {
        terminal_write_string("  Error: /etc/shadow no existe.\n");
        return;
    }

    remove_user_from_vfs_file("/etc/shadow", args);
    remove_user_from_vfs_file("/etc/passwd", args);

    terminal_write_string("  Usuario eliminado.\n");
}

static void cmd_passwd(const char* args) {
    char username[32] = {0};
    char password[32] = {0};

    const char* ptr = args;
    int idx = 0;
    while (*ptr && *ptr != ' ' && idx < 31) {
        username[idx++] = *ptr++;
    }
    if (*ptr == ' ') {
        ptr++;
        idx = 0;
        while (*ptr && idx < 31) {
            password[idx++] = *ptr++;
        }
    }

    if (username[0] == '\0' || password[0] == '\0') {
        terminal_write_string("  Uso: user passwd <usuario> <nuevo_password>\n");
        return;
    }

    fs_node_t* shadow_node = vfs_lookup(fs_root, "/etc/shadow");
    if (!shadow_node) {
        terminal_write_string("  Error: /etc/shadow no existe.\n");
        return;
    }

    /* SHA256 */
    uint8_t hash[SHA256_BLOCK_SIZE];
    sha256((const uint8_t*)password, strlen(password), hash);
    char hash_str[SHA256_BLOCK_SIZE * 2 + 1];
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        char hex[3];
        utoa_hex_s(hash[i], hex, sizeof(hex));
        if (hash[i] < 16) {
            hash_str[i*2] = '0';
            hash_str[i*2+1] = hex[0];
        } else {
            hash_str[i*2] = hex[0];
            hash_str[i*2+1] = hex[1];
        }
    }
    hash_str[SHA256_BLOCK_SIZE * 2] = '\0';

    uint8_t* buffer = kmalloc(shadow_node->length + 1);
    read_fs(shadow_node, 0, shadow_node->length, buffer);
    buffer[shadow_node->length] = '\0';

    fs_node_t* etc_node = vfs_lookup(fs_root, "/etc");
    create_fs(etc_node, "shadow.tmp", 0600);
    fs_node_t* tmp_node = vfs_lookup(fs_root, "/etc/shadow.tmp");
    if (tmp_node) {
        if (tmp_node->truncate) tmp_node->truncate(tmp_node, 0);
        else tmp_node->length = 0;
        tmp_node->mask = 0600;
    }

    char* tptr = (char*)buffer;
    int found = 0;
    while (*tptr) {
        char line[MAX_LINE] = {0};
        int l = 0;
        while (*tptr && *tptr != '\n' && l < MAX_LINE - 1) {
            line[l++] = *tptr++;
        }
        if (*tptr == '\n') line[l++] = *tptr++;
        line[l] = '\0';

        char user[32] = {0};
        char rest_of_line[MAX_LINE] = {0};
        int field = 0, c_idx = 0, r_idx = 0;

        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == ':') {
                field++;
                if (field == 1) continue;
                if (field == 2) {
                    rest_of_line[r_idx++] = ':';
                    continue;
                }
            }
            if (field == 0 && c_idx < 31) user[c_idx++] = line[i];
            else if (field >= 2 && r_idx < MAX_LINE - 1) {
                if (line[i] != '\n') rest_of_line[r_idx++] = line[i];
            }
        }

        if (strcmp(user, username) == 0) {
            found = 1;
            char entry[MAX_LINE];
            int e_len = 0;
            for (int i = 0; user[i] != '\0' && e_len < MAX_LINE - 1; i++) entry[e_len++] = user[i];
            if (e_len < MAX_LINE - 1) entry[e_len++] = ':';
            for (int i = 0; hash_str[i] != '\0' && e_len < MAX_LINE - 1; i++) entry[e_len++] = hash_str[i];

            if (r_idx == 0) {
                // Fallback (exactly 8 colons in a standard entry: username:hash:19000:0:99999:7:::)
                entry[e_len++] = ':'; entry[e_len++] = '1'; entry[e_len++] = '9'; entry[e_len++] = '0'; entry[e_len++] = '0'; entry[e_len++] = '0';
                entry[e_len++] = ':'; entry[e_len++] = '0'; entry[e_len++] = ':'; entry[e_len++] = '9'; entry[e_len++] = '9'; entry[e_len++] = '9'; entry[e_len++] = '9'; entry[e_len++] = '9';
                entry[e_len++] = ':'; entry[e_len++] = '7'; entry[e_len++] = ':'; entry[e_len++] = ':'; entry[e_len++] = ':';
            } else {
                 for (int i = 0; rest_of_line[i] != '\0' && e_len < MAX_LINE - 1; i++) entry[e_len++] = rest_of_line[i];
            }

            if (e_len < MAX_LINE - 1) entry[e_len++] = '\n';
            entry[e_len] = '\0';
            write_fs(tmp_node, tmp_node->length, e_len, (uint8_t*)entry);
        } else {
            write_fs(tmp_node, tmp_node->length, l, (uint8_t*)line);
        }
    }
    kfree(buffer);

    if (found) {
        uint8_t* new_buf = kmalloc(tmp_node->length);
        read_fs(tmp_node, 0, tmp_node->length, new_buf);
        if (shadow_node->truncate) shadow_node->truncate(shadow_node, 0);
        else shadow_node->length = 0;
        write_fs(shadow_node, 0, tmp_node->length, new_buf);
        kfree(new_buf);
        terminal_write_string("  Contrasena actualizada.\n");
    } else {
        terminal_write_string("  Error: Usuario no encontrado.\n");
    }
}

void cmd_user(const char* args) {
    if (!args || *args == '\0') {
        terminal_write_string("\n  Uso de 'user':\n");
        terminal_write_string("    user autologin <on|off>  - Habilitar/Deshabilitar auto-login de root\n");
        terminal_write_string("    user add <user> <pass>   - Crear nuevo usuario\n");
        terminal_write_string("    user del <user>          - Eliminar usuario\n");
        terminal_write_string("    user passwd <user> <pwd> - Cambiar contrasena\n\n");
        return;
    }

    if (strncmp(args, "add ", 4) == 0) {
        cmd_useradd(args + 4);
    } else if (strncmp(args, "del ", 4) == 0) {
        cmd_userdel(args + 4);
    } else if (strncmp(args, "passwd ", 7) == 0) {
        cmd_passwd(args + 7);
    } else if (strncmp(args, "autologin on", 12) == 0) {
        fs_node_t* autologin_node = vfs_lookup(fs_root, "/etc/autologin");
        if (!autologin_node) {
            fs_node_t* etc_node = vfs_lookup(fs_root, "/etc");
            if (!etc_node) {
                if (vfs_mkdir("/etc", 0755) != 0) {
                     terminal_write_string("  Error: No se pudo crear directorio /etc\n");
                     return;
                }
                etc_node = vfs_lookup(fs_root, "/etc");
            }
            if (etc_node) create_fs(etc_node, "autologin", 0644);
            autologin_node = vfs_lookup(fs_root, "/etc/autologin");
        }
        if (autologin_node) {
            write_fs(autologin_node, 0, 1, (uint8_t*)"1");
            terminal_write_string("  [OK] Auto-login habilitado.\n");
        } else {
            terminal_write_string("  Error: No se pudo abrir/crear /etc/autologin\n");
        }
    } else if (strncmp(args, "autologin off", 13) == 0) {
        fs_node_t* autologin_node = vfs_lookup(fs_root, "/etc/autologin");
        if (autologin_node) {
            write_fs(autologin_node, 0, 1, (uint8_t*)"0");
            terminal_write_string("  [OK] Auto-login deshabilitado.\n");
        } else {
             terminal_write_string("  Error: No se pudo abrir /etc/autologin\n");
        }
    } else {
         terminal_write_string("  Opcion no reconocida.\n");
    }
}

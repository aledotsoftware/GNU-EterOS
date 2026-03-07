#include "shell_internal.h"
#include "../../include/fs/vfs.h"

void cmd_user(const char* args) {
    if (!args || *args == '\0') {
        terminal_write_string("\n  Uso de 'user':\n");
        terminal_write_string("    user autologin <on|off>  - Habilitar/Deshabilitar auto-login de root\n\n");
        return;
    }

    if (strncmp(args, "autologin on", 12) == 0) {
        fs_node_t* autologin_node = vfs_lookup(fs_root, "/etc/autologin");
        if (!autologin_node) {
            /* Crear /etc/ si no existe */
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

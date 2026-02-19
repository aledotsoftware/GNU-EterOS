#include "shell_internal.h"
#include "../../include/net/e1000.h"
#include "../../include/net/dhcp.h"

void cmd_net(const char* args) {
    (void)args;
    terminal_write_string("Network disabled.\n");
}

void cmd_dhcp(const char* args) {
    (void)args;
    terminal_write_string("Network disabled.\n");
}

void cmd_wget(const char* args) {
    (void)args;
    terminal_write_string("Network disabled.\n");
}

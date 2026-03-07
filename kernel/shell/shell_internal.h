#ifndef SHELL_INTERNAL_H
#define SHELL_INTERNAL_H

#include "../../include/shell.h"
#include "../../include/vga.h"
#include "../../include/string.h"

// History
void shell_history_init(void);
void shell_history_add(const char* line);
const char* shell_history_get(unsigned int index);
unsigned int shell_history_count(void);

// Commands logic
const char* match_command(const char* input, const char* cmd);
const char* shell_autocomplete(const char* prefix);

// Command handlers
void cmd_help(const char* args);
void cmd_clear(const char* args);
void cmd_version(const char* args);
void cmd_sysinfo(const char* args);
void cmd_echo(const char* args);
void cmd_about(const char* args);
void cmd_reboot(const char* args);
void cmd_halt(const char* args);
void cmd_mem(const char* args);
void cmd_lspci(const char* args);
void cmd_net(const char* args);
void cmd_dhcp(const char* args);
void cmd_uptime(const char* args);
void cmd_ps(const char* args);
void cmd_kill(const char* args);
void cmd_demo(const char* args);
void cmd_date(const char* args);
void cmd_usermode(const char* args);
void cmd_wget(const char* args);
void cmd_user(const char* args);
void cmd_test_compositor(const char* args);
void wget_run(const char* url_in);
void cmd_ota(const char* args);

void cmd_keymap(const char* args);
void cmd_typematic(const char* args);
void cmd_mouse(const char* args);
void cmd_storage(const char* args);

void cmd_ntp(const char* args);
void cmd_time(const char* args);
void cmd_timezone(const char* args);

#endif

#include "shell_internal.h"
#include "../../include/net/nic.h"
#include "../../include/net/dhcp.h"
#include "../../include/net/defs.h"
#include "../../include/stdio.h"

void cmd_net(const char* args) {
    (void)args;
    terminal_write_string("==== Network Status ====\n");
    if (!current_nic) {
        terminal_write_string("Error: Network adapter not active or not detected.\n");
        return;
    }

    uint8_t* mac = current_nic->get_mac();

    char buf[64];
    snprintf(buf, sizeof(buf), "MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    terminal_write_string(buf);

    if (network_ready) {
        terminal_write_string("State: UP\n");

        snprintf(buf, sizeof(buf), "IP Address: %d.%d.%d.%d\n",
            (my_ip >> 24) & 0xFF, (my_ip >> 16) & 0xFF, (my_ip >> 8) & 0xFF, my_ip & 0xFF);
        terminal_write_string(buf);

        snprintf(buf, sizeof(buf), "Gateway: %d.%d.%d.%d\n",
            (gateway_ip >> 24) & 0xFF, (gateway_ip >> 16) & 0xFF, (gateway_ip >> 8) & 0xFF, gateway_ip & 0xFF);
        terminal_write_string(buf);

        snprintf(buf, sizeof(buf), "DNS: %d.%d.%d.%d\n",
            (dns_ip >> 24) & 0xFF, (dns_ip >> 16) & 0xFF, (dns_ip >> 8) & 0xFF, dns_ip & 0xFF);
        terminal_write_string(buf);
    } else {
        terminal_write_string("State: DOWN (Link or DHCP pending)\n");
        terminal_write_string("\n\n\n");
    }
    // Pad output to always be exactly 6 lines after the title
    if (!network_ready) {
        // Output already has 1 line (MAC) + 1 line (State) + 3 newlines = 5 lines. Add 1 more.
        terminal_write_string("\n");
    }
}

#include "../../include/timer.h"
#include "../../include/task.h"

void cmd_dhcp(const char* args) {
    (void)args;
    if (!current_nic) {
        terminal_write_string("Error: Network adapter not active or not detected.\n");
        return;
    }

    terminal_write_string("Waiting for IP address...\n");

    for (int i = 0; i < 50; i++) {
        if (network_ready && my_ip != 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "DHCP Bound! IP Address: %d.%d.%d.%d\n",
                (my_ip >> 24) & 0xFF, (my_ip >> 16) & 0xFF, (my_ip >> 8) & 0xFF, my_ip & 0xFF);
            terminal_write_string(buf);
            return;
        }
        task_yield();
        task_sleep(100);
    }

    terminal_write_string("Requesting new DHCP lease...\n");
    net_dhcp_renew();

    for (int i = 0; i < 50; i++) {
        if (network_ready && my_ip != 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "DHCP Bound! IP Address: %d.%d.%d.%d\n",
                (my_ip >> 24) & 0xFF, (my_ip >> 16) & 0xFF, (my_ip >> 8) & 0xFF, my_ip & 0xFF);
            terminal_write_string(buf);
            return;
        }
        task_yield();
        task_sleep(100);
    }

    terminal_write_string("DHCP timeout.\n");
}
void cmd_wget(const char* args) {
    if (!current_nic) {
        terminal_write_string("Error: Network adapter not active or not detected.\n");
        return;
    }
    if (!network_ready) {
        terminal_write_string("Error: Network is not ready. Please wait for DHCP or configure IP manually.\n");
        return;
    }
    if (!args || args[0] == '\0') {
        terminal_write_string("Usage: wget <url>\n");
        return;
    }
    wget_run(args);
}

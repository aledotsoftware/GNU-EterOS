#include "shell_internal.h"
#include "../../include/net/e1000.h"
#include "../../include/net/dhcp.h"
#include "../../include/net/defs.h"
#include "../../include/stdio.h"

void cmd_net(const char* args) {
    (void)args;
    terminal_write_string("==== Network Status ====\n");
    uint8_t* mac = e1000_get_mac();

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
        terminal_write_string("State: DOWN\n");
    }
}

void cmd_dhcp(const char* args) {
    (void)args;
    terminal_write_string("Requesting DHCP...\n");
    dhcp_discover();
    terminal_write_string("DHCP Discovery sent.\n");
}

void cmd_wget(const char* args) {
    if (!args || args[0] == '\0') {
        terminal_write_string("Usage: wget <url>\n");
        return;
    }
    wget_run(args);
}

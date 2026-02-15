#include <string.h>
#include <serial.h>
#include <vga.h>
#include <hal.h>
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/init.h"
#include "lwip/netif.h"

/* State for the wget request */
typedef struct {
    struct tcp_pcb *pcb;
    const char *hostname;
    const char *path;
    int complete;
} wget_state_t;

static wget_state_t current_request;

/* Forward declarations */
static err_t wget_connected(void *arg, struct tcp_pcb *pcb, err_t err);
static err_t wget_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
static void wget_error(void *arg, err_t err);
static err_t wget_poll(void *arg, struct tcp_pcb *pcb);
static void wget_dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg);

/**
 * Parses URL into hostname and path.
 * Modifies the input string temporarily if needed or copies?
 * We assume simple format: "hostname/path" or "hostname"
 * If "http://" is present, skip it.
 */
static void parse_url(char* url, char** host, char** path) {
    if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        terminal_write_string("Error: HTTPS not supported yet.\n");
        *host = NULL;
        return;
    }

    *host = url;
    char* slash = strchr(url, '/');
    if (slash) {
        *slash = '\0'; /* Split host and path */
        *path = slash + 1;
    } else {
        *path = "";
    }
}

static void wget_close(struct tcp_pcb *pcb) {
    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_close(pcb);
    current_request.complete = 1;
}

static err_t wget_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg;
    if (err == ERR_OK && p != NULL) {
        tcp_recved(pcb, p->tot_len);

        /* Print received data to console */
        /* Note: pbuf might be chained, but for simplicity we iterate */
        struct pbuf *q;
        for (q = p; q != NULL; q = q->next) {
            /* Be careful not to overwhelm VGA */
            /* Using a loop to print chars handling newlines */
            char* data = (char*)q->payload;
            for (u16_t i = 0; i < q->len; i++) {
                terminal_putchar(data[i]);
            }
        }
        pbuf_free(p);
    } else {
        if (err != ERR_OK) {
            terminal_write_string("\n[WGET] Error receiving data.\n");
        } else {
            terminal_write_string("\n[WGET] Connection closed by remote host.\n");
        }
        wget_close(pcb);
    }
    return ERR_OK;
}

static err_t wget_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK) {
        terminal_write_string("[WGET] Connection failed.\n");
        wget_close(pcb);
        return err;
    }

    terminal_write_string("[WGET] Connected. Sending request...\n");

    /* Construct HTTP Request */
    char buffer[512];
    /* Safe snprintf replacement or manual construction */
    /* Since we don't have full libc, we build it manually or use simple sprintfs if available */
    /* Assuming we have strlcpy/strlcat */

    /* "GET /path HTTP/1.0\r\nHost: hostname\r\nUser-Agent: eterOS/0.1\r\n\r\n" */
    /* Note: path must start with / if empty */
    const char* path_prefix = (current_request.path[0] == '\0') ? "/" : "";

    // Manual construction to avoid missing snprintf
    buffer[0] = 0;
    strlcat(buffer, "GET /", sizeof(buffer));
    strlcat(buffer, current_request.path, sizeof(buffer));
    strlcat(buffer, " HTTP/1.0\r\nHost: ", sizeof(buffer));
    strlcat(buffer, current_request.hostname, sizeof(buffer));
    strlcat(buffer, "\r\nUser-Agent: eterOS/0.1\r\n\r\n", sizeof(buffer));

    tcp_write(pcb, buffer, strlen(buffer), TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);

    return ERR_OK;
}

static void wget_error(void *arg, err_t err) {
    (void)arg;
    (void)err;
    terminal_write_string("\n[WGET] TCP Error occurred.\n");
    current_request.complete = 1;
}

static err_t wget_poll(void *arg, struct tcp_pcb *pcb) {
    (void)arg;
    /* Timeout handling could go here */
    return ERR_OK;
}

static void wget_dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    (void)name;
    (void)callback_arg;

    if (ipaddr != NULL && ipaddr->addr != 0) {
        char buf[32];
        terminal_write_string("[WGET] Host resolved: ");
        ip4addr_ntoa_r(ipaddr, buf, sizeof(buf));
        terminal_write_string(buf);
        terminal_write_string("\n");

        current_request.pcb = tcp_new();
        if (!current_request.pcb) {
            terminal_write_string("[WGET] Error creating PCB.\n");
            current_request.complete = 1;
            return;
        }

        tcp_arg(current_request.pcb, NULL);
        tcp_err(current_request.pcb, wget_error);
        tcp_recv(current_request.pcb, wget_recv);
        // tcp_poll(current_request.pcb, wget_poll, 4);

        if (tcp_connect(current_request.pcb, ipaddr, 80, wget_connected) != ERR_OK) {
            terminal_write_string("[WGET] Error connecting.\n");
            current_request.complete = 1;
        }
        tcp_output(current_request.pcb);
    } else {
        terminal_write_string("[WGET] DNS Resolution failed.\n");
        current_request.complete = 1;
    }
}

void wget_run(const char* url_in) {
    char url_buf[256];
    char *host = NULL;
    char *path = NULL;

    /* Copy to modifiable buffer */
    strlcpy(url_buf, url_in, sizeof(url_buf));
    parse_url(url_buf, &host, &path);

    if (!host) return;

    terminal_write_string("[WGET] Resolving ");
    terminal_write_string(host);
    terminal_write_string("...\n");

    /* Setup state */
    current_request.hostname = host;
    current_request.path = path;
    current_request.complete = 0;

    hal_interrupts_disable();
    ip_addr_t dns_res;
    err_t err = dns_gethostbyname(host, &dns_res, wget_dns_found, NULL);

    if (err == ERR_OK) {
        /* Cached immediately */
        wget_dns_found(host, &dns_res, NULL);
    }
    hal_interrupts_enable();

    if (err == ERR_OK || err == ERR_INPROGRESS) {
        /* Wait for callback */
        /* Since we are in NO_SYS, the main loop (network_task) handles polling.
           But we are in a shell command, which blocks.
           We need to loop here calling network polling functions until done. */

        /* NOTE: This busy wait inside a shell command is not ideal for multitasking
           but fine for this demo. We must yield to allow network_task to run if it's separate,
           OR we must poll the stack ourselves if we ARE the main thread.

           In main.c we have a 'network_task' running. If we block here, we block the shell task.
           If the network stack runs in 'network_task', we just need to yield.
        */

        int timeout = 10000; /* ~10 seconds */
        while (!current_request.complete && timeout > 0) {
            /* Yield to let network_task process packets */
            /* Assuming we have task_yield() available from task.h */
            extern void task_yield(void);
            extern void sys_check_timeouts(void);

            /* If the shell is the ONLY task running network (unlikely if we spawned a task),
               we might need to poll here. But we spawned 'network_task'.
               So just yielding is enough. */
            task_yield();

            /* Simple delay */
            for(volatile int i=0; i<10000; i++);
            timeout--;
        }

        if (!current_request.complete) {
            terminal_write_string("\n[WGET] Operation timed out.\n");
        }

    } else {
        terminal_write_string("[WGET] DNS Error.\n");
    }
}

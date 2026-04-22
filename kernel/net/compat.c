/**
 * éterOS - lwIP Compatibility Layer
 *
 * Provides legacy API support (raw_tcp_get) using lwIP 2.2.0
 */

#include <string.h>
#include <hal.h>
#include <net/e1000.h>
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/ip_addr.h"
#include "lwip/dhcp.h"
#include "netif/ethernet.h"
#include "../lwip_port/ethernetif.h"
#include <task.h>
#include <mm.h>

/* Global network state for legacy compatibility */
volatile int network_ready = 0;
uint32_t my_ip = 0;
uint32_t gateway_ip = 0;
uint32_t dns_ip = 0;
uint8_t gateway_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static struct netif main_netif;

void net_init(void) {
    serial_write_string("[NET] Initializing lwIP...\n");
    lwip_init();

    ip_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 0,0,0,0);
    IP4_ADDR(&netmask, 0,0,0,0);
    IP4_ADDR(&gw, 0,0,0,0);

    serial_write_string("[NET] Adding netif...\n");
    /* Add network interface */
    if (netif_add(&main_netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, ethernet_input) == NULL) {
        serial_write_string("[NET] Failed to add netif\n");
        return;
    }

    serial_write_string("[NET] Setting default/up...\n");
    netif_set_default(&main_netif);
    netif_set_up(&main_netif);

    serial_write_string("[NET] Starting DHCP...\n");
    /* Start DHCP */
    dhcp_start(&main_netif);

    /* Set fallback DNS (8.8.8.8) */
    ip_addr_t dns_fallback;
    IP4_ADDR(&dns_fallback, 8, 8, 8, 8);
    dns_setserver(0, &dns_fallback);

    serial_write_string("[NET] Init done.\n");
}

void net_poll(void) {
    /* Poll the driver */
    ethernetif_poll(&main_netif);

    /* Handle lwIP timeouts */
    sys_check_timeouts();

    /* Update global status */
    if (netif_is_up(&main_netif) && !ip_addr_isany(&main_netif.ip_addr)) {
        my_ip = ip4_addr_get_u32(&main_netif.ip_addr);
        gateway_ip = ip4_addr_get_u32(&main_netif.gw);
        const ip_addr_t *dns = dns_getserver(0);
        if (dns) {
            dns_ip = ip4_addr_get_u32(dns);
        }
        network_ready = 1;
    } else {
        network_ready = 0;
    }
}

/* State for blocking operations */
typedef struct {
    struct tcp_pcb *pcb;
    char* response_buf;
    size_t max_len;
    size_t received_len;
    int status; /* 0=pending, 1=success, -1=error */
    volatile int completed;
    char req_buf[512]; /* Request buffer inside state for safety */
    volatile int orphaned; /* Set if caller times out and leaves us dangling */
} http_req_state_t;

static void http_close(struct tcp_pcb *pcb) {
    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_err(pcb, NULL);
    tcp_poll(pcb, NULL, 0);
    tcp_close(pcb);
}

static void http_error(void *arg, err_t err) {
    http_req_state_t *state = (http_req_state_t*)arg;
    (void)err;
    state->status = -1;
    state->completed = 1;
}

static err_t http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    http_req_state_t *state = (http_req_state_t*)arg;

    if (err == ERR_OK && p != NULL) {
        tcp_recved(pcb, p->tot_len);

        struct pbuf *q;
        for (q = p; q != NULL; q = q->next) {
            size_t copy_len = q->len;
            if (state->received_len + copy_len >= state->max_len) {
                copy_len = state->max_len - state->received_len - 1;
            }
            if (copy_len > 0) {
                memcpy(state->response_buf + state->received_len, q->payload, copy_len);
                state->received_len += copy_len;
            }
        }
        pbuf_free(p);

    } else {
        /* Connection closed or error */
        if (err == ERR_OK) {
            state->status = 1; /* Success (Closed by server) */
        } else {
            state->status = -1;
        }
        http_close(pcb);
        state->completed = 1;
    }
    return ERR_OK;
}

static err_t http_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    http_req_state_t *state = (http_req_state_t*)arg;

    if (err != ERR_OK) {
        state->status = -1;
        state->completed = 1;
        http_close(pcb);
        return err;
    }

    /* Send GET request */
    tcp_write(pcb, state->req_buf, strlen(state->req_buf), TCP_WRITE_FLAG_COPY);
    tcp_output(pcb); /* Flush */

    return ERR_OK;
}

static void dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    (void)name;
    http_req_state_t *state = (http_req_state_t*)callback_arg;

    /* If request was cancelled/timed out, free state and return */
    if (state->orphaned) {
        kfree(state);
        return;
    }

    if (ipaddr && ipaddr->addr != 0) {
        state->pcb = tcp_new();
        if (!state->pcb) {
            state->status = -1;
            state->completed = 1;
            return;
        }

        tcp_arg(state->pcb, state);
        tcp_err(state->pcb, http_error);
        tcp_recv(state->pcb, http_recv);

        if (tcp_connect(state->pcb, ipaddr, 80, http_connected) != ERR_OK) {
            state->status = -1;
            state->completed = 1;
        }
    } else {
        state->status = -1; /* DNS fail */
        state->completed = 1;
    }
}

int raw_tcp_get(const char* host, const char* path, char* response_buf, size_t max_len) {
    /* Allocate state on heap to avoid Use-After-Return if timeout occurs */
    http_req_state_t *state = (http_req_state_t*)kmalloc(sizeof(http_req_state_t));
    if (!state) return -1;

    memset(state, 0, sizeof(*state));
    state->response_buf = response_buf;
    state->max_len = max_len;
    state->status = 0;
    state->completed = 0;
    state->orphaned = 0;

    /* Prepare request */
    state->req_buf[0] = 0;
    strlcpy(state->req_buf, "GET ", sizeof(state->req_buf));
    if (path[0] != '/') strlcat(state->req_buf, "/", sizeof(state->req_buf));
    strlcat(state->req_buf, path, sizeof(state->req_buf));
    strlcat(state->req_buf, " HTTP/1.0\r\nHost: ", sizeof(state->req_buf));
    strlcat(state->req_buf, host, sizeof(state->req_buf));
    strlcat(state->req_buf, "\r\nUser-Agent: eterOS/0.1\r\n\r\n", sizeof(state->req_buf));

    /* Start DNS - PROTECTED */
    hal_interrupts_disable();
    ip_addr_t dns_res;
    err_t err = dns_gethostbyname(host, &dns_res, dns_found, state);

    if (err == ERR_OK) {
        dns_found(host, &dns_res, state);
    } else if (err != ERR_INPROGRESS) {
        hal_interrupts_enable();
        kfree(state);
        return -1; /* DNS Error immediately */
    }
    hal_interrupts_enable();

    /* Wait loop (max ~10 seconds) */
    int timeout_ms = 10000;

    while (!state->completed && timeout_ms > 0) {
        task_sleep(1);
        timeout_ms--;
    }

    int result_status = state->status;
    size_t result_len = state->received_len;

    if (!state->completed) {
        /* Timeout - Abort connection safely */
        hal_interrupts_disable();
        if (state->pcb) {
            tcp_abort(state->pcb);
            state->pcb = NULL;
            /* If PCB was active, tcp_abort kills it. We can free state safely. */
            kfree(state);
        } else {
            /* DNS Pending or other state. Cannot cancel easily.
               Orphan the state so callback frees it. */
            state->orphaned = 1;
        }
        hal_interrupts_enable();
        return -2; /* Timeout */
    }

    /* Null terminate response if space allows */
    if (result_len < max_len) {
        response_buf[result_len] = 0;
    } else {
        response_buf[max_len - 1] = 0;
    }

    kfree(state);
    return (result_status == 1) ? (int)result_len : result_status;
}

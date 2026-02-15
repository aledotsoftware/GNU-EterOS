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
#include <task.h>

/* State for blocking operations */
typedef struct {
    struct tcp_pcb *pcb;
    char* response_buf;
    size_t max_len;
    size_t received_len;
    int status; /* 0=pending, 1=success, -1=error */
    volatile int completed;
    char req_buf[512]; /* Request buffer inside state for safety */
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
    http_req_state_t state;

    /* Initialize state on stack */
    memset((void*)&state, 0, sizeof(state));
    state.response_buf = response_buf;
    state.max_len = max_len;
    state.status = 0;
    state.completed = 0;

    /* Prepare request */
    state.req_buf[0] = 0;
    strlcpy(state.req_buf, "GET ", sizeof(state.req_buf));
    if (path[0] != '/') strlcat(state.req_buf, "/", sizeof(state.req_buf));
    strlcat(state.req_buf, path, sizeof(state.req_buf));
    strlcat(state.req_buf, " HTTP/1.0\r\nHost: ", sizeof(state.req_buf));
    strlcat(state.req_buf, host, sizeof(state.req_buf));
    strlcat(state.req_buf, "\r\nUser-Agent: eterOS/0.1\r\n\r\n", sizeof(state.req_buf));

    /* Start DNS - PROTECTED */
    hal_interrupts_disable();
    ip_addr_t dns_res;
    err_t err = dns_gethostbyname(host, &dns_res, dns_found, &state);

    if (err == ERR_OK) {
        dns_found(host, &dns_res, &state);
    } else if (err != ERR_INPROGRESS) {
        hal_interrupts_enable();
        return -1; /* DNS Error immediately */
    }
    hal_interrupts_enable();

    /* Wait loop (max ~10 seconds) */
    int timeout_ticks = 10000;

    while (!state.completed && timeout_ticks > 0) {
        task_yield();
        /* Simple delay */
        for(volatile int i=0; i<10000; i++);
        timeout_ticks--;
    }

    if (!state.completed) {
        /* Timeout - Abort connection safely */
        hal_interrupts_disable();
        if (state.pcb) {
            tcp_abort(state.pcb);
        }
        hal_interrupts_enable();
        return -2; /* Timeout */
    }

    /* Null terminate response if space allows */
    if (state.received_len < max_len) {
        response_buf[state.received_len] = 0;
    } else {
        response_buf[max_len - 1] = 0;
    }

    return (state.status == 1) ? (int)state.received_len : state.status;
}

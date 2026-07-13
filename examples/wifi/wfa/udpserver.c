#include <stdarg.h>

#include "FreeRTOS.h"
#include <lwip/tcpip.h>
#include <lwip/api.h>
#include <lwip/sys.h>
#include <lwip/udp.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#include "shell.h"

#define UDP_ECHO_SERVER_PORT    (7777)

static struct udp_pcb *g_udp_echo_pcb = NULL;

static inline void udp_log(const char *fmt, ...)
{
    va_list va;
    char s[256];

    puts("[UDP-Server] ");
    va_start(va, fmt);
    vsnprintf(s, sizeof(s) - 1, fmt, va);
    va_end(va);
    puts(s);
}

static void udpecho_raw_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                             const ip_addr_t *addr, u16_t port)
{
    LWIP_UNUSED_ARG(arg);
    if (p == NULL) {
        return;
    }

    struct pbuf *q = pbuf_alloc(PBUF_TRANSPORT, p->tot_len, PBUF_RAM);
    if (q == NULL) {
        pbuf_free(p);
        return;
    }

    if (pbuf_copy(q, p) != ERR_OK) {
        pbuf_free(q);
        pbuf_free(p);
        return;
    }

    udp_sendto(pcb, q, addr, port);

    pbuf_free(q);
    pbuf_free(p);
}

static void udp_echo_server_stop_internal(void *arg)
{
    LWIP_UNUSED_ARG(arg);
    if (g_udp_echo_pcb != NULL) {
        udp_recv(g_udp_echo_pcb, NULL, NULL);
        udp_remove(g_udp_echo_pcb);
        g_udp_echo_pcb = NULL;
    }
}

static void udp_echo_server_start(void *arg)
{
    err_t ret;
    struct udp_pcb *pcb;

    LWIP_UNUSED_ARG(arg);
    printf("starting %s() ..........\n", __func__);

    /* release previous instance if any */
    udp_echo_server_stop_internal(NULL);

    pcb = udp_new();
    if (NULL == pcb) {
        return;
    }
    ret = udp_bind(pcb, IP_ADDR_ANY, UDP_ECHO_SERVER_PORT);
    if (ERR_OK != ret) {
        printf("bind socket error. Maybe %d already binded?", UDP_ECHO_SERVER_PORT);
        udp_remove(pcb);
        return;
    }
    /* no need to loop forever */
    udp_recv(pcb, (udp_recv_fn)udpecho_raw_recv, pcb);
    g_udp_echo_pcb = pcb;
}

int cmd_udp_echo_server(int argc, char **argv)
{
    int ret;

    ret = tcpip_callback((tcpip_callback_fn)udp_echo_server_start, NULL);
    udp_log("udp_echo_server_start with ret %d", ret);

    return 0;
}

int cmd_udp_echo_server_stop(int argc, char **argv)
{
    int ret;

    ret = tcpip_callback((tcpip_callback_fn)udp_echo_server_stop_internal, NULL);
    udp_log("udp_echo_server_stop with ret %d", ret);

    return 0;
}
SHELL_CMD_EXPORT_ALIAS(cmd_udp_echo_server, udps, create a udp echo server with raw api.);
SHELL_CMD_EXPORT_ALIAS(cmd_udp_echo_server_stop, udps_stop, stop the udp echo server.);

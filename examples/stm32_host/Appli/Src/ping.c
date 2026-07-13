/*
 * netutils: ping implementation
 *
 * Host-side copy of components/net/lwip/lwip_apps/ping/ping.c, adapted for
 * the STM32 host shell.
 */

#include <FreeRTOS.h>
#include <task.h>

#include <lwip/opt.h>
#include <lwip/init.h>
#include <lwip/mem.h>
#include <lwip/icmp.h>
#include <lwip/netif.h>
#include <lwip/sys.h>
#include <lwip/inet.h>
#include <lwip/inet_chksum.h>
#include <lwip/ip.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/**
 * PING_DEBUG: Enable debugging for PING.
 */
#ifndef PING_DEBUG
#define PING_DEBUG LWIP_DBG_ON
#endif

/** ping receive timeout - in milliseconds */
#define PING_RCV_TIMEO (2000 * portTICK_PERIOD_MS)
/** ping delay - in milliseconds */
#define PING_DELAY     (1000 * portTICK_PERIOD_MS)

/** ping identifier - must fit on a u16_t */
#ifndef PING_ID
#define PING_ID 0xAFAF
#endif

/** ping additional data size to include in the packet */
#ifndef PING_DATA_SIZE
#define PING_DATA_SIZE 32
#endif

#define PING_MAX_DATA_SIZE 1472U
#define PING_RECV_BUF_SIZE (IP_HLEN_MAX + sizeof(struct icmp_echo_hdr) + PING_MAX_DATA_SIZE)

/* ping variables */
static u16_t ping_seq_num;
static char ping_recv_buf[PING_RECV_BUF_SIZE];

/** Prepare a echo ICMP request */
static void ping_prepare_echo(struct icmp_echo_hdr *iecho, u16_t len)
{
    size_t i;
    size_t data_len = len - sizeof(struct icmp_echo_hdr);

    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id = PING_ID;
    iecho->seqno = htons(++ping_seq_num);

    /* fill the additional data buffer with some data */
    for (i = 0; i < data_len; i++) {
        ((char *)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
    }

    iecho->chksum = inet_chksum(iecho, len);
}

/* Ping using the socket ip */
err_t lwip_ping_send(int s, ip_addr_t *addr, int size)
{
    int err;
    struct icmp_echo_hdr *iecho;
    struct sockaddr_in to;
    int ping_size = sizeof(struct icmp_echo_hdr) + size;
    LWIP_ASSERT("ping_size is too big", ping_size <= 0xffff);

    iecho = mem_malloc(ping_size);
    if (iecho == NULL) {
        return ERR_MEM;
    }

    ping_prepare_echo(iecho, (u16_t)ping_size);

    memset(&to, 0, sizeof(to));
    to.sin_len = sizeof(to);
    to.sin_family = AF_INET;
#if LWIP_IPV4 && LWIP_IPV6
    to.sin_addr.s_addr = ip_2_ip4(addr)->addr;
#elif LWIP_IPV4
    to.sin_addr.s_addr = addr->addr;
#elif LWIP_IPV6
#error Not supported IPv6.
#endif

    err = lwip_sendto(s, iecho, ping_size, 0, (struct sockaddr *)&to, sizeof(to));
    mem_free(iecho);

    return (err == ping_size ? ERR_OK : ERR_VAL);
}

int lwip_ping_recv(int s, int *ttl)
{
    socklen_t fromlen = sizeof(struct sockaddr_in);
    int len;
    struct sockaddr_in from;
    struct ip_hdr *iphdr;
    struct icmp_echo_hdr *iecho;

    while ((len = lwip_recvfrom(s, ping_recv_buf, sizeof(ping_recv_buf), 0,
                                (struct sockaddr *)&from, &fromlen)) > 0) {
        if (len >= (int)(IP_HLEN + sizeof(struct icmp_echo_hdr))) {
            u8_t iphdr_len;

            iphdr = (struct ip_hdr *)ping_recv_buf;
            iphdr_len = IPH_HL_BYTES(iphdr);
            if (iphdr_len < IP_HLEN ||
                len < (int)(iphdr_len + sizeof(struct icmp_echo_hdr))) {
                continue;
            }

            iecho = (struct icmp_echo_hdr *)(ping_recv_buf + iphdr_len);
            if ((ICMPH_TYPE(iecho) == ICMP_ER) && (iecho->id == PING_ID) &&
                (iecho->seqno == htons(ping_seq_num))) {
                *ttl = IPH_TTL(iphdr);
                return len;
            }
        }
    }

    return len;
}

/* using the lwIP custom ping */
uint32_t ping(char *target_name, uint16_t interval, uint16_t size, uint32_t count)
{
#if LWIP_VERSION_MAJOR >= 2U
    struct timeval timeout = { PING_RCV_TIMEO / (1000 * portTICK_PERIOD_MS),
                               (PING_RCV_TIMEO % (1000 * portTICK_PERIOD_MS)) * 1000 };
#else
    int timeout = PING_RCV_TIMEO * 1000UL / (1000 * portTICK_PERIOD_MS);
#endif

    int s, ttl, recv_len;
    ip_addr_t target_addr;
    uint32_t send_times;
    uint32_t recv_times;
    uint32_t recv_start_tick;
    struct addrinfo hint, *res = NULL;
    struct sockaddr_in *h = NULL;
    struct in_addr ina;
    ip4_addr_t ip4;
    char target_ip[IP4ADDR_STRLEN_MAX];

    send_times = 0;
    recv_times = 0;
    ping_seq_num = 0;

    if (size == 0) {
        size = PING_DATA_SIZE;
    }

    if (size > PING_MAX_DATA_SIZE || count == 0) {
        printf("ping: invalid argument\n");
        return 1;
    }

    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_INET;
    /* convert URL to IP */
    if (lwip_getaddrinfo(target_name, NULL, &hint, &res) != 0 || res == NULL || res->ai_addr == NULL) {
        if (res != NULL) {
            lwip_freeaddrinfo(res);
        }
        printf("ping: unknown host %s\n\r", target_name);
        return 1;
    }

    h = (struct sockaddr_in *)res->ai_addr;
    memcpy(&ina, &h->sin_addr, sizeof(ina));
    ip4.addr = ina.s_addr;
#if LWIP_IPV4 && LWIP_IPV6
    ip_addr_copy_from_ip4(target_addr, ip4);
#else
    target_addr.addr = ip4.addr;
#endif
    (void)inet_ntoa_r(ina, target_ip, sizeof(target_ip));
    lwip_freeaddrinfo(res);

    /* new a socket */
    if ((s = lwip_socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP)) < 0) {
        printf("ping: create socket failed\n\r");
        return 1;
    }

    lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (1) {
        int elapsed_time;

        if (lwip_ping_send(s, &target_addr, size) == ERR_OK) {
            recv_start_tick = sys_now();
            if ((recv_len = lwip_ping_recv(s, &ttl)) >= 0) {
                recv_times++;
                elapsed_time = (int)(sys_now() - recv_start_tick);
                printf("%d bytes from %s icmp_seq=%lu ttl=%d time=%d ms\n\r",
                       recv_len, target_ip, (unsigned long)send_times, ttl,
                       elapsed_time);
            } else {
                printf("From %s icmp_seq=%lu timeout\n\r", target_ip, (unsigned long)send_times);
            }
        } else {
            printf("Send %s - error\n\r", target_ip);
        }

        send_times++;
        if (send_times >= count) {
            /* send ping times reached, stop */
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(interval)); /* take a delay */
    }

    lwip_close(s);
    printf("Ping Done. Statistic is: %lu sent %lu received\n",
           (unsigned long)send_times, (unsigned long)recv_times);

    return 0;
}

#define PING_USAGE \
"ping [-c count] [-i interval] [-s size] [-h help] destination\r\n" \
"\t\t-c count of ping requests. default is 4\r\n" \
"\t\t-i interval in ms. default is 1000\r\n" \
"\t\t-s ICMP payload size in bytes. default is 32\r\n" \
"\t\t-h print this help\r\n"

static bool ping_parse_u32(const char *str, uint32_t *value)
{
    char *end;
    unsigned long parsed;

    if (str[0] == '\0' || str[0] == '-') {
        return false;
    }

    errno = 0;
    parsed = strtoul(str, &end, 0);
    if (errno == ERANGE || end == str || *end != '\0' || parsed > UINT32_MAX) {
        return false;
    }

    *value = (uint32_t)parsed;
    return true;
}

void cmd_ping(int argc, char **argv)
{
    uint16_t interval = PING_DELAY;
    uint16_t data_size = PING_DATA_SIZE;
    uint32_t total_count = 4;
    const char *target = NULL;

    if (argc == 1) {
        goto usage;
    }

    for (int i = 1; i < argc; i++) {
        uint32_t value;

        if (strcmp(argv[i], "-h") == 0) {
            goto usage;
        } else if (strcmp(argv[i], "-i") == 0 ||
                   strcmp(argv[i], "-s") == 0 ||
                   strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc || !ping_parse_u32(argv[i + 1], &value)) {
                goto usage;
            }

            if (strcmp(argv[i], "-i") == 0) {
                if (value == 0 || value > UINT16_MAX) {
                    goto usage;
                }
                interval = (uint16_t)value;
            } else if (strcmp(argv[i], "-s") == 0) {
                if (value > PING_MAX_DATA_SIZE) {
                    goto usage;
                }
                data_size = (uint16_t)value;
            } else {
                total_count = value;
            }
            i++;
        } else if (target == NULL) {
            target = argv[i];
        } else {
            goto usage;
        }
    }

    if (target == NULL || interval == 0 || total_count == 0 ||
        data_size > PING_MAX_DATA_SIZE) {
        goto usage;
    }

    (void)ping((char *)target, interval, data_size, total_count);
    return;

usage:
    printf("%s", PING_USAGE);
}

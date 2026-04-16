#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <lwip/ip4_addr.h>
#include "net_iperf_al_priv.h"

void iperf_start(uint8_t is_client, uint8_t is_udp, const char *ip)
{
	uint32_t dest_ip = 0;
    struct fhost_iperf_settings iperf_settings;
    bool client_server_set = 0;

    fhost_iperf_settings_init(&iperf_settings);
    dest_ip = ipaddr_addr(ip);
    if (is_udp) {
        if (!iperf_settings.flags.is_udp) {
            iperf_settings.flags.is_udp = true;
            iperf_settings.udprate = FHOST_IPERF_DEFAULT_UDPRATE;
        }

        // if -l has already been processed, is_buf_len_set is true, so don't overwrite that value.
        if (!iperf_settings.flags.is_buf_len_set) {
            iperf_settings.buf_len = FHOST_IPERF_DEFAULT_UDPBUFLEN;
        }
    }

    // UDP TX
    if (is_udp && is_client) {
        iperf_settings.udprate = 100 * 1000 * 1000;
        iperf_settings.flags.is_udp = true;
        iperf_settings.flags.is_bw_set = true;
        // if -l has already been processed, is_buf_len_set is true so don't overwrite that value.
        iperf_settings.buf_len = FHOST_IPERF_DEFAULT_UDPBUFLEN;
    }

    if (is_client) {
        iperf_settings.flags.is_server = 0;
        client_server_set = true;
        iperf_settings.host_ip = dest_ip;
    } else {
        iperf_settings.flags.is_server = 1;
        client_server_set = true;
    }

    uint32_t interval = 10;
    iperf_settings.interval.sec = interval / 10;
    iperf_settings.interval.usec = 100000 * (interval - (iperf_settings.interval.sec * 10));
    iperf_settings.flags.show_int_stats = true;
        
    iperf_settings.flags.is_time_mode = true;
    iperf_settings.amount = 100;
       
    fhost_iperf_start(&iperf_settings);
}


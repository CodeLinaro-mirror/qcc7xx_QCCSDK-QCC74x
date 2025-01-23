/*
 * app_atmodule.h
 *
 *  Created on: Jun 3, 2024
 */

#ifndef SRC_APP_ATMODULE_H_
#define SRC_APP_ATMODULE_H_

#include "at_host.h"

typedef struct ota_header {
    union {
        struct {
            uint8_t header[16];

            uint8_t type[4]; //RAW XZ
            uint32_t len;    //body len
            uint8_t pad0[8];

            uint8_t ver_hardware[16];
            uint8_t ver_software[16];

            uint8_t sha256[32];
        } s;
        uint8_t _pad[512];
    } u;
} at_ota_header_t;

int at_iperf_udp_tx_start(at_host_handle_t at, char ip_addr[20], int port);
int at_iperf_tcp_tx_start(at_host_handle_t at, char ip_addr[20], int port);
int at_iperf_udp_rx_start(at_host_handle_t at, char ip_addr[20], int port);
int at_iperf_tcp_rx_start(at_host_handle_t at, char ip_addr[20], int port);

at_host_handle_t at_spisync_init(void);

#endif /* SRC_APP_ATMODULE_H_ */

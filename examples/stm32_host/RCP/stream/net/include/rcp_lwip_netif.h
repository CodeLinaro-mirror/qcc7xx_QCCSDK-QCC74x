#ifndef __NETIF_RCP_H__
#define __NETIF_RCP_H__

#include <lwip/netif.h>
#include <rcp_bus.h>
#include <rcp_protocol.h>
#include <stdbool.h>

err_t rcp_lwip_init(void);
err_t rcp_lwip_dhcp_start(uint32_t timeout_ms);
err_t rcp_lwip_dhcp_done(void);
err_t rcp_lwip_dhcpd_start(void);
err_t rcp_lwip_dhcpd_stop(void);

/* Get the RCP network interface */
struct netif* rcp_netif_get(void);
struct netif* rcp_netif_ap_get(void);

#endif

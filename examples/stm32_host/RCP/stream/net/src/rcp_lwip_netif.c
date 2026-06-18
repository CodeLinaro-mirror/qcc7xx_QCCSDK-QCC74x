#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <buffer_mgmt_ctrl.h>
#include <rcp_config.h>
#include <rcp_interface.h>
#include <rcp_os.h>
#include <rcp_stat.h>

#include "rcp_lwip_netif.h"
#include "log.h"
#include <lwip/dhcp.h>
#include <lwip/etharp.h>
#include <lwip/ip4_addr.h>
#include <lwip/netif.h>
#include <lwip/netifapi.h>
#include <lwip/pbuf.h>
#include <lwip/tcpip.h>
#include <dhcp_server.h>
#include <wifi/rcp_wifi_mgmt.h>
#include <qcc74x_sdio.h>

#define RCP_LWIP_DEBUG 0

/* Standard ETH MAC address length */
#define NETIF_RCP_ADDR_LEN 6
#define NETIF_RCP_STA_HOSTNAME "RCP STA"
#define NETIF_RCP_AP_HOSTNAME  "RCP AP"
/* Timeout to receive MAC address from device */
#define MAC_ADDR_TIMEOUT_MS 1000

/* Make sure RCP buffer size is large enough for MTU + RCP overhead */
#if (RCP_BUF_SIZE < 1536)
#error "RCP Buffer size to small (<1536) for RCP lwip netif driver"
#endif

#define NETIF_RCP_MTU 1500
#define RCP_ETH_NUM_BUFFERS RCP_ETH_TX_NUM
#define RCP_WIFI_SUBTYPE_STA 0
#define RCP_WIFI_SUBTYPE_AP  1
#define RCP_AP_DHCP_START    2
#define RCP_AP_DHCP_LIMIT    100
#define RCP_LWIP_TX_HEADROOM (sizeof(buffer_mgmt_elem_t) + sizeof(rcp_pro_packt_t))
#define RCP_LWIP_WORD_MASK   (sizeof(uint32_t) - 1U)

typedef struct {
	struct netif netif;
	lnk_sts_t *lnk_sts;
	uint8_t stream_id;
	uint8_t msg_subtype;
	char ifname[2];
	const char *hostname;
	bool added;
	bool dhcpd_started;
} rcp_lwip_port_t;

typedef struct {
	rcp_lwip_port_t sta;
	rcp_lwip_port_t ap;
	lnk_sts_t *lnk_sts;
	rcp_timer_t dhcp_timer;
	bool dhcp_timer_initialized;
	bool dhcp_waiting;
} rcp_lwip_ctx_t;

static rcp_lwip_ctx_t g_rcp_lwip = {
	.sta = {
		.stream_id = RCP_MSG_TYPE_WIFI_STA,
		.msg_subtype = RCP_WIFI_SUBTYPE_STA,
		.ifname = {'Q', 'C'},
		.hostname = NETIF_RCP_STA_HOSTNAME,
	},
	.ap = {
		.stream_id = RCP_MSG_TYPE_WIFI_STA,
		.msg_subtype = RCP_WIFI_SUBTYPE_AP,
		.ifname = {'Q', 'A'},
		.hostname = NETIF_RCP_AP_HOSTNAME,
	},
};

#if !defined(LWIP_ARP)
#error "RCP driver requires LWIP_ARP"
#endif

_Static_assert(FRAME_BUFF_PBUFCUSTOM >= sizeof(struct pbuf_custom),
	       "FRAME_BUFF_PBUFCUSTOM is too small for lwIP custom pbuf");
_Static_assert(PBUF_LINK_ENCAPSULATION_HLEN >= RCP_LWIP_TX_HEADROOM,
	       "PBUF_LINK_ENCAPSULATION_HLEN is too small for zero-copy TX");
_Static_assert(ETH_PAD_SIZE == 2,
	       "RCP lwIP zero-copy TX requires ETH_PAD_SIZE=2 for SDIO DMA alignment");
_Static_assert((sizeof(buffer_mgmt_elem_t) & RCP_LWIP_WORD_MASK) == 0U,
	       "buffer_mgmt_elem_t must keep TX headroom word aligned");
_Static_assert((sizeof(rcp_pro_packt_t) & RCP_LWIP_WORD_MASK) == 0U,
	       "rcp_pro_packt_t must keep TX headroom word aligned");

static rcp_lwip_port_t *rcp_lwip_default_port(void);
static rcp_lwip_port_t *rcp_lwip_port_from_subtype(uint8_t msg_subtype);
static void rcp_lwip_make_ap_mac(uint8_t mac[NETIF_RCP_ADDR_LEN]);
static struct pbuf *rcp_lwip_pbuf_from_elem(const buffer_mgmt_elem_t *payload_ptr);
static void rcp_lwip_zero_copy_free(struct pbuf *p);
static void rcp_lwip_tx_pbuf_free(buffer_mgmt_elem_t *buffer_elem, void *arg);
static buffer_mgmt_elem_t *rcp_lwip_tx_zero_copy_prepare(struct pbuf *p);
static err_t rcp_lwip_add_port(rcp_lwip_port_t *port, bool set_default);

static uint8_t handle_rcp_msg(rcp_message_t *pmessage, lnk_sts_t *lnk);
static err_t netif_rcp_output(struct netif *netif, struct pbuf *p);
static void __sta_status_callback(struct netif *netif);
static void rcp_lwip_dhcp_timeout(void *arg);

void debug_print_ethernet_packet(const char *tag, struct pbuf *p_buf) {
    if (p_buf == NULL || p_buf->payload == NULL) {
        printf("pbuf is NULL or has no payload\n");
        return;
    }

    struct eth_hdr *ethhdr = (struct eth_hdr *)p_buf->payload;
    uint16_t payload_len = p_buf->tot_len - sizeof(struct eth_hdr);
    uint8_t *payload = (uint8_t *)p_buf->payload + sizeof(struct eth_hdr);

    printf("%s Destination MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n", tag,
           ethhdr->dest.addr[0], ethhdr->dest.addr[1], ethhdr->dest.addr[2],
           ethhdr->dest.addr[3], ethhdr->dest.addr[4], ethhdr->dest.addr[5]);

    printf("Source MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           ethhdr->src.addr[0], ethhdr->src.addr[1], ethhdr->src.addr[2],
           ethhdr->src.addr[3], ethhdr->src.addr[4], ethhdr->src.addr[5]);

    printf("EtherType: 0x%04X\n", ntohs(ethhdr->type));

    switch (ntohs(ethhdr->type)) {
        case ETHTYPE_IP:
            printf("Payload Type: IPv4\r\n");
            break;
        case ETHTYPE_ARP:
            printf("Payload Type: ARP\r\n");
            break;
        case ETHTYPE_IPV6:
            printf("Payload Type: IPv6\r\n");
            break;
        default:
            printf("Payload Type: Unknown (0x%04X)\r\n", ntohs(ethhdr->type));
            break;
    }

    payload_len = p_buf->len;
    payload = (uint8_t *)p_buf->payload;

    printf("tot_len: %d bytes\r\n", payload_len);

//    uint16_t len = (payload_len < 128) ? payload_len : 128;
//    uint16_t len = payload_len;
//    for (uint16_t i = 0; i < len; i++) {
//        printf("%02X ", payload[i]);
//        if ((i + 1) % 16 == 0) printf("\r\n");
//    }

//    if (payload_len > 128) {
//    	printf("...\n");
//        uint8_t *payload_end = payload + payload_len - 128;
//        for (uint16_t i = 0; i < 128; i++) {
//            printf("%02X ", payload_end[i]);
//            if ((i + 1) % 16 == 0) printf("\n");
//        }
//    }
}

static rcp_lwip_port_t *rcp_lwip_default_port(void)
{
	return &g_rcp_lwip.sta;
}

static rcp_lwip_port_t *rcp_lwip_port_from_subtype(uint8_t msg_subtype)
{
	if (msg_subtype == RCP_WIFI_SUBTYPE_AP) {
		return &g_rcp_lwip.ap;
	}

	return &g_rcp_lwip.sta;
}

static void rcp_lwip_make_ap_mac(uint8_t mac[NETIF_RCP_ADDR_LEN])
{
	static const uint8_t fallback_mac[NETIF_RCP_ADDR_LEN] = {
		0x02, 0x00, 0x00, 0x74, 0x00, 0x01
	};

	if (rcp_wifi_get_mac(mac, 0U)) {
		mac[0] = (uint8_t)((mac[0] | 0x02U) & 0xfeU);
		mac[5] ^= 0x01U;
		return;
	}

	memcpy(mac, fallback_mac, sizeof(fallback_mac));
}

static void rcp_lwip_zero_copy_free(struct pbuf *p)
{
	buffer_mgmt_elem_t *buffer;

	if (p == NULL) {
		return;
	}

	buffer = (buffer_mgmt_elem_t *)((uint8_t *)p - FRAME_BUFF_ELEM_SIZE);
	(void)buffer_mgmt_free_elem(buffer);
}

static struct pbuf *rcp_lwip_pbuf_from_elem(const buffer_mgmt_elem_t *payload_ptr)
{
	buffer_mgmt_elem_t *payload = (buffer_mgmt_elem_t *)payload_ptr;
	struct pbuf_custom *custom;

	if (payload == NULL || payload->data_size == 0U || payload->offset < ETH_PAD_SIZE) {
		return NULL;
	}

	custom = (struct pbuf_custom *)((uint8_t *)payload->buff_addr + FRAME_BUFF_ELEM_SIZE);
	custom->custom_free_function = rcp_lwip_zero_copy_free;

	return pbuf_alloced_custom(PBUF_RAW,
				   (uint16_t)(buffer_mgmt_data_size(payload) + ETH_PAD_SIZE),
				   PBUF_REF, custom,
				   buffer_mgmt_data_ptr(payload) - ETH_PAD_SIZE,
				   (uint16_t)(buffer_mgmt_max_size(payload) + ETH_PAD_SIZE));
}

static void rcp_lwip_tx_pbuf_free(buffer_mgmt_elem_t *buffer_elem, void *arg)
{
	(void)buffer_elem;
	pbuf_free((struct pbuf *)arg);
}

static buffer_mgmt_elem_t *rcp_lwip_tx_zero_copy_prepare(struct pbuf *p)
{
	buffer_mgmt_elem_t *msg_buffer;
	uint8_t *payload;
	uint32_t xfer_len;

	if (p == NULL || p->payload == NULL || p->next != NULL || p->len != p->tot_len) {
		return NULL;
	}

	xfer_len = (uint32_t)p->tot_len + sizeof(rcp_pro_packt_t);
	if (xfer_len > QCC74x_SDIO_BLOCK_SIZE) {
		xfer_len = ((xfer_len + QCC74x_SDIO_BLOCK_SIZE - 1U) /
			    QCC74x_SDIO_BLOCK_SIZE) * QCC74x_SDIO_BLOCK_SIZE;
		if (xfer_len > QCC74x_SDIO_MAX_MSG_SIZE) {
			return NULL;
		}
	}
		
	payload = (uint8_t *)p->payload;
	if (((uintptr_t)payload & RCP_LWIP_WORD_MASK) != 0U) {
		return NULL;
	}

	msg_buffer = (buffer_mgmt_elem_t *)(payload - RCP_LWIP_TX_HEADROOM);
	msg_buffer->buffer_ctrl = NULL;
	msg_buffer->buff_addr = msg_buffer;
	msg_buffer->offset = (uint32_t)(payload - (uint8_t *)msg_buffer);
	msg_buffer->data_size = p->tot_len;
	msg_buffer->max_size = (uint16_t)(msg_buffer->offset - sizeof(rcp_pro_packt_t) + xfer_len);
	msg_buffer->free_cb = rcp_lwip_tx_pbuf_free;
	msg_buffer->cb_arg = p;

	return msg_buffer;
}

static err_t netif_rcp_init(struct netif *netif)
{
	rcp_lwip_port_t *port = (rcp_lwip_port_t *)netif->state;

	if (port == NULL) {
		return ERR_IF;
	}

	/* Current context */
#if LWIP_NETIF_HOSTNAME
	netif->hostname = port->hostname;
#endif

	netif->name[0] = port->ifname[0];
	netif->name[1] = port->ifname[1];
	netif->hwaddr_len = NETIF_RCP_ADDR_LEN;

	if (port->msg_subtype == RCP_WIFI_SUBTYPE_AP) {
		rcp_lwip_make_ap_mac(netif->hwaddr);
	} else if (!rcp_wifi_get_mac(netif->hwaddr, MAC_ADDR_TIMEOUT_MS)) {
		LOG_ERR("Failed to get interface MAC from rcp_wifi");
		return ERR_IF;
	}

	netif->output = etharp_output;
	netif->linkoutput = netif_rcp_output;

	netif->mtu = NETIF_RCP_MTU;

	netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;

	return ERR_OK;
}

static void __sta_status_callback(struct netif *netif)
{
	rcp_lwip_port_t *port = (rcp_lwip_port_t *)netif->state;

	if (ip4_addr_isany(netif_ip4_addr(netif))) {
	} else {
		(void)rcp_lwip_dhcp_done();
		printf("%c%c IP Address: %s\r\n", netif->name[0], netif->name[1],
		       ipaddr_ntoa(netif_ip4_addr(netif)));
		printf("Netmask:    %s\r\n", ipaddr_ntoa(netif_ip4_netmask(netif)));
		printf("Gateway:    %s\r\n", ipaddr_ntoa(netif_ip4_gw(netif)));
	}
}

static void __ap_status_callback(struct netif *netif)
{
	rcp_lwip_port_t *port = (rcp_lwip_port_t *)netif->state;

	if (ip4_addr_isany(netif_ip4_addr(netif))) {

	} else {

		printf("%c%c STA IP Address: %s\r\n", netif->name[0], netif->name[1],
		       ipaddr_ntoa(netif_ip4_addr(netif)));
		//printf("Netmask:    %s\r\n", ipaddr_ntoa(netif_ip4_netmask(netif)));
		//printf("Gateway:    %s\r\n", ipaddr_ntoa(netif_ip4_gw(netif)));
	}
}
static void rcp_lwip_dhcp_timeout(void *arg)
{
	(void)arg;
	g_rcp_lwip.dhcp_waiting = false;
	LOG_WRN("DHCP timed out, disconnecting WiFi");
	(void)rcp_wifi_disconnect();
}

err_t rcp_lwip_dhcp_start(uint32_t timeout_ms)
{
	rcp_lwip_port_t *port = rcp_lwip_default_port();
	err_t err;

	if (port == NULL) {
		return ERR_IF;
	}

#if LWIP_DHCP
	if (!g_rcp_lwip.dhcp_timer_initialized) {
		if (!rcp_os_timer_init(&g_rcp_lwip.dhcp_timer, rcp_lwip_dhcp_timeout, port)) {
			LOG_ERR("Failed to initialize DHCP timer");
			return ERR_IF;
		}
		g_rcp_lwip.dhcp_timer_initialized = true;
	}

	err = netifapi_netif_set_up(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set netif up (%d)", err);
		return err;
	}

	err = netifapi_netif_set_link_up(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set link up (%d)", err);
		return err;
	}

	err = netifapi_dhcp_release(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to release previous DHCP lease (%d)", err);
		return err;
	}

	err = netifapi_dhcp_start(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to start DHCP (%d)", err);
		return err;
	}

	if (timeout_ms > 0U) {
		if (!rcp_os_timer_start(&g_rcp_lwip.dhcp_timer, timeout_ms)) {
			LOG_ERR("Failed to start DHCP timer");
			return ERR_IF;
		}
		g_rcp_lwip.dhcp_waiting = true;
	} else {
		g_rcp_lwip.dhcp_waiting = false;
	}
#else
	err = netifapi_netif_set_up(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set netif up (%d)", err);
		return err;
	}

	err = netifapi_netif_set_link_up(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set link up (%d)", err);
		return err;
	}
#endif

	return ERR_OK;
}

err_t rcp_lwip_dhcp_done(void)
{
#if LWIP_DHCP
	if (g_rcp_lwip.dhcp_timer_initialized && g_rcp_lwip.dhcp_waiting) {
		(void)rcp_os_timer_stop(&g_rcp_lwip.dhcp_timer);
	}
	//(void)netifapi_dhcp_release_and_stop(&g_rcp_lwip);
#endif
	//(void)netifapi_netif_set_link_down(&g_rcp_lwip);
	//(void)netifapi_netif_set_down(&g_rcp_lwip);
	g_rcp_lwip.dhcp_waiting = false;
	return ERR_OK;
}

err_t rcp_lwip_init(void)
{
	err_t err;
	rcp_lwip_port_t *sta = &g_rcp_lwip.sta;
	rcp_lwip_port_t *ap = &g_rcp_lwip.ap;
	const rcp_stream_config_t conf = {
		.tx_cnt = RCP_ETH_NUM_BUFFERS,
		.rx_wnd = RCP_ETH_RX_WND + 2,
		.rx_threshold = 6,
		.tx_threshold = 6,
	};

	if (link_register_rx_handler(RCP_MSG_TYPE_WIFI_STA, handle_rcp_msg) != RCP_LNK_SUCCESS) {
		LOG_ERR("Failed to register ETH RX handler");
		return ERR_IF;
	}

	if (g_rcp_lwip.lnk_sts == NULL) {
		g_rcp_lwip.lnk_sts = rcp_stream_create(sta->stream_id, &conf);
		if (g_rcp_lwip.lnk_sts == NULL) {
			LOG_ERR("Failed to create ETH stream");
			return ERR_IF;
		}
	}
	sta->lnk_sts = g_rcp_lwip.lnk_sts;
	ap->lnk_sts = g_rcp_lwip.lnk_sts;

	tcpip_init(NULL, NULL);

	err = rcp_lwip_add_port(sta, true);
	if (err != ERR_OK) {
		return err;
	}

	err = rcp_lwip_add_port(ap, false);
	if (err != ERR_OK) {
		return err;
	}

	return ERR_OK;
}

struct netif* rcp_netif_get(void)
{
	return &rcp_lwip_default_port()->netif;
}

struct netif *rcp_netif_ap_get(void)
{
	return &g_rcp_lwip.ap.netif;
}

static err_t rcp_lwip_add_port(rcp_lwip_port_t *port, bool set_default)
{
	err_t err;

	if (port == NULL) {
		return ERR_ARG;
	}

	if (port->added) {
		return ERR_OK;
	}

	err = netifapi_netif_add(&port->netif, NULL, NULL, NULL, port, &netif_rcp_init, &tcpip_input);
	if (err != ERR_OK) {
		LOG_ERR("Failed to add RCP lwIP netif (%d)", err);
		return err;
	}
	port->added = true;
	netif_set_status_callback(&port->netif, __sta_status_callback);

	if (set_default) {
		err = netifapi_netif_set_default(&port->netif);
		if (err != ERR_OK) {
			LOG_ERR("Failed to set default netif (%d)", err);
			return err;
		}
	}

	err = netifapi_netif_set_link_down(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set link down (%d)", err);
		return err;
	}

	err = netifapi_netif_set_down(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set netif down (%d)", err);
		return err;
	}

	return ERR_OK;
}

err_t rcp_lwip_dhcpd_start(void)
{
	rcp_lwip_port_t *port = &g_rcp_lwip.ap;
	ip4_addr_t ip;
	ip4_addr_t netmask;
	ip4_addr_t gw;
	uint8_t mac[NETIF_RCP_ADDR_LEN];
	err_t err;

	if (!port->added) {
		return ERR_IF;
	}

	if (rcp_wifi_ap_get_mac_address(mac) == RCP_WIFI_OK) {
		memcpy(port->netif.hwaddr, mac, sizeof(mac));
	}

	IP4_ADDR(&ip, 192, 168, 169, 1);
	IP4_ADDR(&netmask, 255, 255, 255, 0);
	IP4_ADDR(&gw, 192, 168, 169, 1);

	err = netifapi_netif_set_up(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set AP netif up (%d)", err);
		return err;
	}

	err = netifapi_netif_set_link_up(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set AP link up (%d)", err);
		return err;
	}

	err = netifapi_netif_set_addr(&port->netif, &ip, &netmask, &gw);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set AP netif address (%d)", err);
		return err;
	}

	dhcpd_start(&port->netif, RCP_AP_DHCP_START, RCP_AP_DHCP_LIMIT);

    rcp_os_delay_ms(200);
    dhcpd_status_callback_set(&port->netif, __ap_status_callback);
	port->dhcpd_started = true;
	return ERR_OK;
}

err_t rcp_lwip_dhcpd_stop(void)
{
	rcp_lwip_port_t *port = &g_rcp_lwip.ap;
	err_t err;

	if (!port->added) {
		return ERR_OK;
	}

	if (port->dhcpd_started) {
		dhcpd_stop_with_netif(&port->netif);
		port->dhcpd_started = false;
	}

	err = netifapi_netif_set_link_down(&port->netif);
	if (err != ERR_OK) {
		return err;
	}

	err = netifapi_netif_set_down(&port->netif);
	if (err != ERR_OK) {
		return err;
	}

	return ERR_OK;
}

static uint8_t handle_rcp_msg(rcp_message_t *pmessage, lnk_sts_t *lnk)
{
	struct pbuf *p = NULL;
	rcp_lwip_port_t *port;
	const buffer_mgmt_elem_t *payload_ptr = pmessage ? pmessage->msg_buffer : NULL;

	(void)lnk;

	if ((pmessage != NULL) && (pmessage->msg_flags == RCP_FLAG_DEVICE_START) &&
	    (payload_ptr != NULL) && (payload_ptr->data_size >= NETIF_RCP_ADDR_LEN)) {

		rcp_wifi_set_mac(buffer_mgmt_data_ptr_const(payload_ptr));
		memcpy(g_rcp_lwip.sta.netif.hwaddr, buffer_mgmt_data_ptr_const(payload_ptr),
		       NETIF_RCP_ADDR_LEN);
		return RCP_RX_NO_ACTION;
	}

	if (pmessage == NULL || pmessage->msg_flags != RCP_FLAG_UPLD_DATA || !payload_ptr) {
		return RCP_RX_NO_ACTION;
	}

	port = rcp_lwip_port_from_subtype(pmessage->msg_subtype);

	p = rcp_lwip_pbuf_from_elem(payload_ptr);
	if (p == NULL) {
		LOG_ERR("Failed to allocate zero-copy pbuf for RX packet");
		return RCP_RX_NO_ACTION;
	}
#if RCP_LWIP_DEBUG
	debug_print_ethernet_packet("RX", p);
#endif
	if (port->netif.input == NULL || port->netif.input(p, &port->netif) != ERR_OK) {
		pbuf_free(p);
		return RCP_RX_TAKEN;
	}
	//pbuf_free(p);
	return RCP_RX_TAKEN;
}

static err_t netif_rcp_output(struct netif *netif, struct pbuf *p)
{
	rcp_lwip_port_t *port = (rcp_lwip_port_t *)netif->state;
	buffer_mgmt_elem_t *msg_buffer = NULL;
	link_result_t res;

	if (port == NULL) {
		return ERR_IF;
	}

	if (port->lnk_sts == NULL) {
		LOG_ERR("ETH link not initialized");
		return ERR_IF;
	}

	msg_buffer = rcp_lwip_tx_zero_copy_prepare(p);
	if (msg_buffer == NULL) {
		LOG_ERR("ETH TX pbuf is not zero-copy capable");
		return ERR_IF;
	}

#if RCP_LWIP_DEBUG
	debug_print_ethernet_packet("TX", p);
#endif
	pbuf_ref(p);
	rcp_message_t tx_msg = {
		.stream = port->stream_id,
		.msg_subtype = port->msg_subtype,
		.msg_flags = RCP_FLAG_DNLD_DATA,
		.msg_buffer = msg_buffer,
	};
	RCP_TX_PROF_BEGIN(netif_link_send_cycles);
	res = link_send_msg(&tx_msg, 6000);
	RCP_TX_PROF_ACC(RCP_TX_PROF_NETIF_LINK_SEND, netif_link_send_cycles);
	if (res != RCP_LNK_SUCCESS) {
		pbuf_free(p);
		return ERR_IF;
	}

	return ERR_OK;
}

#include <string.h>
#include <stdio.h>

#include <buffer_mgmt_ctrl.h>
#include <rcp_config.h>
#include <rcp_interface.h>
#include <rcp_os.h>

#include "rcp_lwip_netif.h"
#include "log.h"
#include <lwip/dhcp.h>
#include <lwip/etharp.h>
#include <lwip/netif.h>
#include <lwip/netifapi.h>
#include <lwip/pbuf.h>
#include <lwip/tcpip.h>
#include <wifi/rcp_wifi_mgmt.h>

#define RCP_LWIP_DEBUG 0

/* Standard ETH MAC address length */
#define NETIF_RCP_ADDR_LEN 6
#define NETIF_RCP_HOSTNAME "RCP host"
/* Timeout to receive MAC address from device */
#define MAC_ADDR_TIMEOUT_MS 1000

/* Make sure RCP buffer size is large enough for MTU + RCP overhead */
#if (RCP_BUF_SIZE < 1536)
#error "RCP Buffer size to small (<1536) for RCP lwip netif driver"
#endif

#define NETIF_RCP_MTU 1500
#define RCP_ETH_NUM_BUFFERS 24

typedef struct {
	struct netif netif;
	buffer_mgmt_ctrl_t *buffer_ctrl;
	lnk_sts_t *lnk_sts;
	uint8_t stream_id;
	char ifname[2];
	const char *hostname;
} rcp_lwip_port_t;

typedef struct {
	rcp_lwip_port_t sta;
	uint8_t sta_buffer_mem[RCP_ETH_NUM_BUFFERS][FRAME_BUFF_TOTALSIZE];
	rcp_timer_t dhcp_timer;
	bool dhcp_timer_initialized;
	bool dhcp_waiting;
} rcp_lwip_ctx_t;

static rcp_lwip_ctx_t g_rcp_lwip = {
	.sta = {
		.stream_id = RCP_MSG_TYPE_ETH,
		.ifname = {'Q', 'C'},
		.hostname = NETIF_RCP_HOSTNAME,
	},
};

#if !defined(LWIP_ARP)
#error "RCP driver requires LWIP_ARP"
#endif

_Static_assert(FRAME_BUFF_PBUFCUSTOM >= sizeof(struct pbuf_custom),
	       "FRAME_BUFF_PBUFCUSTOM is too small for lwIP custom pbuf");

static rcp_lwip_port_t *rcp_lwip_default_port(void);
static struct pbuf *rcp_lwip_pbuf_from_elem(const buffer_mgmt_elem_t *payload_ptr);
static void rcp_lwip_zero_copy_free(struct pbuf *p);

static uint8_t handle_rcp_msg(uint8_t msg_flags, const buffer_mgmt_elem_t *payload_ptr);
static err_t netif_rcp_output(struct netif *netif, struct pbuf *p);
static void netif_status_callback(struct netif *netif);
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

    printf("Source MAC: %02X:%02X:%02X:%02X:%02:%02X\r\n",
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

	if (payload == NULL || payload->data_size == 0U) {
		return NULL;
	}

	custom = (struct pbuf_custom *)((uint8_t *)payload->buff_addr + FRAME_BUFF_ELEM_SIZE);
	memset(custom, 0, sizeof(*custom));
	custom->custom_free_function = rcp_lwip_zero_copy_free;

	return pbuf_alloced_custom(PBUF_RAW, buffer_mgmt_data_size(payload), PBUF_REF, custom,
				   buffer_mgmt_data_ptr(payload),
				   (u16_t)buffer_mgmt_max_size(payload));
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

	if (!rcp_wifi_get_mac(netif->hwaddr, MAC_ADDR_TIMEOUT_MS)) {
		LOG_ERR("Failed to get interface MAC from rcp_wifi");
		return ERR_IF;
	}

	netif->output = etharp_output;
	netif->linkoutput = netif_rcp_output;

	netif->mtu = NETIF_RCP_MTU;

	netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;

	return ERR_OK;
}

static void netif_status_callback(struct netif *netif)
{
	if (ip4_addr_isany(netif_ip4_addr(netif))) {
	} else {
		(void)rcp_lwip_dhcp_done();
		printf("IP Address: %s\r\n", ipaddr_ntoa(netif_ip4_addr(netif)));
		printf("Netmask:    %s\r\n", ipaddr_ntoa(netif_ip4_netmask(netif)));
		printf("Gateway:    %s\r\n", ipaddr_ntoa(netif_ip4_gw(netif)));
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
	rcp_lwip_port_t *port = rcp_lwip_default_port();

	buffer_mgmt_ctrl_init_cfg_t cfg = {
		.frame_buff = g_rcp_lwip.sta_buffer_mem,
		.frame_elem_cnt = RCP_ETH_NUM_BUFFERS,
		.frame_elem_size = sizeof(g_rcp_lwip.sta_buffer_mem[0]),
		.name = "rcp_eth",
	};

	if (port->buffer_ctrl == NULL) {
		port->buffer_ctrl = buffer_mgmt_create(&cfg);
		if (port->buffer_ctrl == NULL) {
			LOG_ERR("Failed to create ETH buffer controller");
			return ERR_IF;
		}
	}

	if (link_register_rx_handler(RCP_MSG_TYPE_ETH, handle_rcp_msg) != RCP_LNK_SUCCESS) {
		LOG_ERR("Failed to register ETH RX handler");
		return ERR_IF;
	}

	if (port->lnk_sts == NULL) {
		port->lnk_sts = rcp_stream_create(port->buffer_ctrl, port->stream_id);
		if (port->lnk_sts == NULL) {
			LOG_ERR("Failed to create ETH stream");
			return ERR_IF;
		}
	}

	tcpip_init(NULL, NULL);


	err = netifapi_netif_add(&port->netif, NULL, NULL, NULL, port, &netif_rcp_init, &tcpip_input);
	if (err != ERR_OK) {
		LOG_ERR("Failed to add RCP lwIP netif (%d)", err);
		return err;
	}
	netif_set_status_callback(&port->netif, netif_status_callback);

	err = netifapi_netif_set_default(&port->netif);
	if (err != ERR_OK) {
		LOG_ERR("Failed to set default netif (%d)", err);
		return err;
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

struct netif* rcp_netif_get(void)
{
	return &rcp_lwip_default_port()->netif;
}

static uint8_t handle_rcp_msg(uint8_t msg_flags, const buffer_mgmt_elem_t *payload_ptr)
{
	struct pbuf *p = NULL;
	rcp_lwip_port_t *port = rcp_lwip_default_port();

	if ((msg_flags == RCP_FLAG_DEVICE_READY) && (payload_ptr != NULL) && (payload_ptr->data_size >= NETIF_RCP_ADDR_LEN)) {
		rcp_wifi_set_mac(buffer_mgmt_data_ptr_const(payload_ptr));
		return RCP_RX_NO_ACTION;
	}

	if (msg_flags != RCP_FLAG_UPLD_DATA || !payload_ptr) {
		return RCP_RX_NO_ACTION;
	}

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
	pbuf_free(p);
	return RCP_RX_TAKEN;
}

static err_t netif_rcp_output(struct netif *netif, struct pbuf *p)
{
	rcp_lwip_port_t *port = (rcp_lwip_port_t *)netif->state;
	buffer_mgmt_elem_t *msg_buffer;
	buffer_mgmt_elem_t *msg_buffer_elem = NULL;
	link_result_t res;
	struct pbuf *q = NULL;
	uint8_t *data_ptr = NULL;
	lnk_sts_t *lnk;

	if (port == NULL) {
		return ERR_IF;
	}

	lnk = port->lnk_sts;
	if (!lnk || !port->buffer_ctrl) {
		LOG_ERR("ETH link not initialized");
		return ERR_IF;
	}

	if (buffer_mgmt_alloc_elem(port->buffer_ctrl, &msg_buffer_elem, 0) != 0) {
		LOG_ERR("Failed to allocate packet buffer");
		return ERR_IF;
	}
	msg_buffer = msg_buffer_elem;

	if (msg_buffer == NULL) {
		LOG_ERR("Failed to allocate packet buffer");
		return ERR_IF;
	}

	if (p->tot_len > BUFFER_MGMT_MAX_SIZE(msg_buffer)) {
		LOG_ERR("packet to large to send over link");
		buffer_mgmt_free_elem(msg_buffer);
		return ERR_IF;
	}

	/* Copy whole packet */
	data_ptr = buffer_mgmt_data_ptr(msg_buffer);
	for (q = p; q != NULL; q = q->next) {
		memcpy(data_ptr, q->payload, q->len);
		data_ptr += q->len;
	}
#if RCP_LWIP_DEBUG
	debug_print_ethernet_packet("TX", p);
#endif
	msg_buffer->data_size = p->tot_len;
	res = link_send(port->stream_id, msg_buffer, 6000);
	if (res != RCP_LNK_SUCCESS) {
		buffer_mgmt_free_elem(msg_buffer);
		return ERR_IF;
	}

	return ERR_OK;
}

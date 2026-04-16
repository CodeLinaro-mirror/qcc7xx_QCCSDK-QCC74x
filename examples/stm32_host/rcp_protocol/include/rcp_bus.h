#ifndef __RCP_BUS_H__
#define __RCP_BUS_H__

#include <stdint.h>

typedef struct rcp_msg_packt {
	uint8_t tag;
	uint8_t sub_tag;
	uint16_t len;
	uint8_t data[0];
} rcp_msg_packt_t;

/* Bus transaction status code. */
enum rcp_bus_status {
	RCP_BUS_OK,              /* Transaction complete. */
	RCP_BUS_ERR_IN_PROGRESS = -1, /* Transaction ongoing. */
	RCP_BUS_ERR_NO_INST = -2,     /* Instance not registered. */
	RCP_BUS_ERR_IO = -3,          /* Hardware error. */
	RCP_BUS_ERR_INVALID = -4,     /* Invalid usage, e.g. NULL pointer. */
	RCP_BUS_ERR_EMPTY = -5,       /* No data. */
	RCP_BUS_ERR_CLOSED = -6,      /* Channel closed. */
	RCP_BUS_ERR_MEMORY = -7,      /* No buffer space left. */
	RCP_BUS_ERR_PEER_MEMORY = -8, /* Peer could not accept data. */
	RCP_BUS_ERR_TIMEOUT = -9,     /* Operation timeout. */
	RCP_BUS_ERR_UNKNOWN = -10,     /* Enum delimeter, do not use. */
};

enum rcp_msg_type {
    RCP_MSG_TYPE_NONE = 0, /**< No message tag */
    RCP_MSG_TYPE_TEST_1, /**< Test message tag 1 */
    RCP_MSG_TYPE_TEST_2, /**< Test message tag 2 */
    RCP_MSG_TYPE_TEST_3, /**< Test message tag 3 */
    RCP_MSG_TYPE_SYS,  /**< System message tag */
    RCP_MSG_TYPE_WIFI, /**< WiFi message tag */
    RCP_MSG_TYPE_ETH, /**< Ethernet message tag */
    RCP_MSG_TYPE_TTY, /**< TTY message tag */
    RCP_MSG_TYPE_NETLINK, /**< Netlink message tag */
    RCP_MSG_TYPE_BT_HCI,
    RCP_MSG_TYPE_MAX /**< Maximum message tag */
};

/* Bus instance. */
enum rcp_bus {
	RCP_BUS_SPI,
	RCP_BUS_SDIO,
	RCP_BUS_UART,
	RCP_BUS_MAX, /* Maximum number of available buses. */
};

enum rcp_bus_evt {
	RCP_BUS_FLAG_TX_PENDING = 0x01,
	RCP_BUS_FLAG_RX_DATA = 0x02,
	RCP_BUS_FLAG_UNKNOWN = 0x04, /* Enum delimeter. */
};

/* Data structure for RX ready event. */
struct rcp_bus_evt_rx_rdy {
	uint16_t rx_len;
};

/* Bus instance-specific capabilities. */
struct rcp_bus_caps {
	/* Bus-specific MTU. */
	uint16_t mtu;
};

/* Bus instance API. */
struct rcp_bus_api {
	enum rcp_bus_status (*open)(void);
	enum rcp_bus_status (*close)(void);
	enum rcp_bus_status (*write)(rcp_msg_packt_t *packt);
	int (*read)(rcp_msg_packt_t *packt);
	uint16_t (*get_rx_len)(void);
	enum rcp_bus_status (*get_caps)(struct rcp_bus_caps *caps);
};

/* Register an API instance for the specific bus. Each bus can
 * be registered only once.
 *
 * inst - bus type.
 * api - pointer to API implementation.
 *
 */
enum rcp_bus_status rcp_bus_register(enum rcp_bus bus, struct rcp_bus_api *api);

/* Open bus instance.
 *
 * bus - bus to open.
 */
enum rcp_bus_status rcp_bus_open(enum rcp_bus bus);

/* Close bus instance.
 *
 * bus - bus to close.
 */
enum rcp_bus_status rcp_bus_close(enum rcp_bus bus);

/* Write using bus instance - blocking.
 *
 * bus - bus to use.
 * tx - tx data buffer.
 * len - amount of tx data.
 */
enum rcp_bus_status rcp_bus_write(enum rcp_bus bus, rcp_msg_packt_t *packt);

/* Read using bus instance - blocking.
 *
 * bus - bus to use.
 * rx - data buffer to store rx data.
 * len - size of rx buffer.
 * nread - actual amount of data read.
 */
int rcp_bus_read(enum rcp_bus bus, rcp_msg_packt_t *packt);

/* Get capabilities of the underlying bus.
 *
 * bus - bus to use.
 * caps - returned capabilities structure.
 */
enum rcp_bus_status rcp_bus_get_caps(enum rcp_bus bus, struct rcp_bus_caps *caps);

/* Wait for flag from bus.
 *
 * Waits for bus flags until all of the specified flags are set or timeout expires.
 * This function can also be used to check flags in a non-blocking manner by setting
 * timeout to 0.
 * Flags are controlled by the bus instance.
 *
 * bus - bus to use.
 * flags - flag bits to check.
 * timeout_ms - timeout in miliseconds. Set to 0 to check flags and return immediately.
 *
 * Returns:
 * - RCP_BUS_OK when flags are set.
 * - RCP_BUS_ERR_TIMEOUT when timeout expires.
 * - RCP_BUS_ERR_INVALID for invalid arguments.
 */
enum rcp_bus_status rcp_bus_wait_for_flags(enum rcp_bus bus, uint8_t flags, uint32_t timeout_ms);

/* Wait until any of the requested bus flags is set.
 *
 * bus - bus to use.
 * flags - flag bits to check.
 * timeout_ms - timeout in milliseconds. Set to 0 to check flags and return immediately.
 * matched_flags - optional pointer that receives currently set matching flags.
 */
enum rcp_bus_status rcp_bus_wait_for_any_flags(enum rcp_bus bus, uint8_t flags, uint32_t timeout_ms,
					       uint8_t *matched_flags);

/* Wait for any bus activity notification. This is used by the event loop to block
 * until RX data is available or new TX work is queued for the bus.
 */
enum rcp_bus_status rcp_bus_wait(enum rcp_bus bus, uint32_t timeout_ms);

/* Signal that new TX work has been queued for the bus. */
void rcp_bus_signal_tx_pending(enum rcp_bus bus);

/* Signal that RX data is available for the bus. */
void rcp_bus_signal_rx_ready(enum rcp_bus bus);

/* Clear bus flags.
 *
 * bus - bus instance to use.
 * clr_flags - flags to clear.
 */
void rcp_bus_clear_flags(enum rcp_bus bus, uint8_t clr_flags);

/* Read current bus flags.
 *
 * bus - bus instance to use.
 * flags - returned current bus flags.
 */
enum rcp_bus_status rcp_bus_get_flags(enum rcp_bus bus, uint8_t *flags);

/* Set / clear bus flags.
 * Internal function only to be used by bus instances.
 *
 * bus - bus instance to use.
 * set_flags - flags to set from bus.
 * clr_flags - flags to clear from bus.
 */
void __rcp_bus_set_flags(enum rcp_bus bus, uint8_t set_flags, uint8_t clr_flags);

/* Get amount of RX data waiting to be read.
 *
 * bus - bus intance to get RX data len.
 */
uint16_t rcp_bus_get_rx_len(enum rcp_bus bus);

/* Utility function to convert status code to human-readable string. */
const char *rcp_bus_status_to_str(enum rcp_bus_status status);

#endif

#ifndef __BT_HCI_H__
#define __BT_HCI_H__

#include <stdint.h>

typedef void (*bt_evt_callback)(uint8_t evt, void *data);

struct bt_hci_cmd {
	uint16_t opcode;
	uint8_t payload_len;
	uint8_t *payload;
};

/* HCI CMD immediate response (0x0E or 0x0F event) */
struct bt_hci_resp {
	uint8_t status;        /* Status (for 0x0F event) */
	uint8_t response_len;  /* Response length (for 0x0E) */
	uint8_t response[255]; /* Response (for 0x0E) */
};

/* Callback for BT asynchronous events */
typedef void (*bt_evt_cb)(uint8_t evt, uint8_t *data, uint8_t len);

bool bt_hci_init(bt_evt_cb evt_cb);

bool bt_hci_cmd_send(struct bt_hci_cmd *cmd, struct bt_hci_resp **resp);

#endif

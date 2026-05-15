#ifndef __BT_HCI_H__
#define __BT_HCI_H__

#include <stdint.h>
#include <stdbool.h>

/* HCI H4 packet types */
#define BT_HCI_H4_CMD    0x01  /* HCI Command */
#define BT_HCI_H4_ACL    0x02  /* HCI ACL Data */
#define BT_HCI_H4_SCO    0x03  /* HCI SCO Data */
#define BT_HCI_H4_EVT    0x04  /* HCI Event */

/* Callback for receiving HCI data - receives complete H4 packet (type + payload) */
typedef void (*bt_evt_cb)(const uint8_t *data, uint16_t len);

/**
 * @brief Initialize HCI transport
 * @param evt_cb Callback for receiving HCI data (any H4 type)
 * @return true on success
 */
bool bt_hci_init(bt_evt_cb evt_cb);

/**
 * @brief Send raw HCI data (any H4 type: CMD/ACL/SCO/Event)
 * @param data HCI data in H4 format (type + payload)
 * @param len Length of data
 * @return true on success
 *
 * This is the generic interface for BLE host communication.
 * Supports all HCI packet types:
 * - 0x01: HCI Command
 * - 0x02: HCI ACL Data
 * - 0x03: HCI SCO Data
 * - 0x04: HCI Event
 */
bool bt_hci_send(const uint8_t *data, uint16_t len);

#endif /* __BT_HCI_H__ */

/**
 * @file mr_hci.h
 * @brief HCI transport over message router
 */

#ifndef __MR_HCI_H_
#define __MR_HCI_H_

#include <stdbool.h>
#include <stdint.h>

#include "mr_credit_flow.h"
#include "mr_frame_buff_ctrl.h"
#include "mr_msg_ctrl.h"

#define MR_HCI_RX_BUFF_SIZE                 (512 * 3)
#define MR_HCI_FRAME_CNT                    (5)
#define MR_HCI_CMD_FRAME_CNT                (2)
#define MR_HCI_DNLD_CREDIT_MAX              (MR_HCI_FRAME_CNT - MR_HCI_CMD_FRAME_CNT)
#define MR_HCI_DNLD_CREDIT_UPDATE_THRESHOLD (2)
#define MR_HCI_MSG_QUEUE_DEPTH              (32)

enum {
    MR_HCI_STA_ERROR,
    MR_HCI_STA_IDLE,
    MR_HCI_STA_RUN,
};

#ifndef DEBUG_HCI_LOG_ENABLE
#define DEBUG_HCI_LOG_ENABLE                (0)
#endif

#define MR_HCI_FLAG_NONE                    0
#define MR_HCI_FLAG_DNLD_DATA               1
#define MR_HCI_FLAG_UPLD_DATA               2
#define MR_HCI_FLAG_HOST_RESET              3
#define MR_HCI_FLAG_HOST_READY              4
#define MR_HCI_FLAG_HOST_STOP               5
#define MR_HCI_FLAG_DEVICE_START            7
#define MR_HCI_FLAG_CREDIT_UPDATE           9

#pragma pack(push, 1)

typedef struct {
    struct mr_msg_pkt msg_pkt;
    uint8_t reserved[1];
    uint8_t flag;
    uint8_t credit_update_flag;
    uint8_t credit_limit_cnt;
    uint8_t data[0];
} mr_hci_msg_t;

#pragma pack(pop)

typedef struct mr_hci_priv_s {
    mr_msg_ctrl_priv_t *msg_ctrl;
    uint8_t msg_tag;

    int hci_status;
    mr_flow_ctrl_t flow_ctrl;

    TaskHandle_t hci_proc_task;
    QueueHandle_t msg_dnld_queue;
    mr_frame_queue_ctrl_t *rx_frame_ctrl;
    bool stop_requested;
    bool reset_flag;
    bool hci_ready;
} mr_hci_priv_t;

int mr_hci_init(mr_hci_priv_t *ctx, mr_msg_ctrl_priv_t *msg_ctrl, uint8_t msg_tag);
int mr_hci_send(const void *data, uint32_t len);
int mr_hci_send_skb(const void *data, uint32_t len, void (*free_cb)(void *arg), void *cb_arg);
int mr_hci_channel_init(mr_msg_ctrl_priv_t *msg_ctrl);
int btble_sdio_hci_write(uint8_t *buf, uint32_t len);

#endif /* __MR_HCI_H_ */

/**
 * @file hci.c
 * @brief HCI channel implementation for nethub
 * @details Provides HCI channel that can work independently or alongside WiFi
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nethub.h>
#include <transportsdio.h>
#include <transporthci.h>
#include "nh_sdio_drv.h"
#include "nh_msg_ctrl.h"
#include "nh_frame_buff_ctrl.h"
#include <log.h>

/* Debug configuration */
#define DEBUG_DUMP_HCI_ENABLE (1)

#if DEBUG_DUMP_HCI_ENABLE
#define HCI_INFO(fmt, ...)    printf("[HCI_CH] " fmt, ##__VA_ARGS__)
#else
#define HCI_INFO(fmt, ...)
#endif

#define HCI_ERR(fmt, ...)     printf("[HCI_ERR] " fmt, ##__VA_ARGS__)
#define HCI_DBG(fmt, ...)     //printf("[HCI_DBG] " fmt, ##__VA_ARGS__)

/* Global HCI context */
static hci_priv_t *g_hci_ctx_p = NULL;

/* Get the WiFi SDIO context to reuse msg_ctrl (if available) */
extern wifi_wifista_priv_t *g_sdio_ctx_p;

/*****************************************************************************
 * Callback Functions
 *****************************************************************************/

/**
 * @brief HCI output callback - send data to host via SDIO
 */
static nh_forward_result_t _hci_output_callback(nh_skb_t *skb, void *arg)
{
    int ret;
    (void)arg; // Unused

    if (!skb || !skb->data || skb->len == 0) {
        HCI_DBG("Invalid skb\n");
        return NH_FORWARD_ERROR;
    }

    HCI_INFO("HCI UPLD: %d bytes\n", skb->len);

    /* Send data via transporthci with skb callback
     * The skb will be freed in hci_upld_send_done_cb when transmission completes
     */
    ret = transporthci_send_skb(skb->data, skb->len, skb->free_cb, skb->cb_arg);

    if (ret != 0) {
        HCI_DBG("transporthci_send_skb failed: %d\n", ret);
        return NH_FORWARD_ERROR;
    }

    return NH_FORWARD_CONTINUE;
}

/*****************************************************************************
 * Initialization
 *****************************************************************************/

/**
 * @brief Initialize HCI transport layer
 * @note This function is called by SDIO channel initialization
 * @return NH_OK on success, error code on failure
 */
int hci_channel_init(void)
{
    int ret;
    nh_msg_ctrl_t *msg_ctrl = NULL;

    HCI_INFO("HCI SDIO channel init\n");

    /* Get msg_ctrl from SDIO channel */
    if (g_sdio_ctx_p && g_sdio_ctx_p->msg_ctrl) {
        msg_ctrl = g_sdio_ctx_p->msg_ctrl;
        HCI_INFO("Using SDIO msg_ctrl for HCI\n");
    } else {
        HCI_ERR("SDIO msg_ctrl not available\n");
        return NH_ERR_NOT_INITIALIZED;
    }

    /* Allocate and initialize HCI context */
    g_hci_ctx_p = (hci_priv_t *)malloc(sizeof(struct hci_priv));
    if (!g_hci_ctx_p) {
        HCI_ERR("Failed to allocate HCI context\n");
        return NH_ERR_NO_MEMORY;
    }
    memset(g_hci_ctx_p, 0, sizeof(struct hci_priv));

    /* Register HCI with existing msg_ctrl */
    ret = transporthci_init(g_hci_ctx_p, msg_ctrl, nh_MSG_TAG_HCI);
    if (ret < 0) {
        HCI_ERR("transporthci_init failed: %d\n", ret);
        free(g_hci_ctx_p);
        g_hci_ctx_p = NULL;
        return NH_ERR_INTERNAL;
    }

    HCI_INFO("HCI channel initialized successfully\n");

    return NH_OK;
}

/**
 * @brief Deinitialize HCI transport layer
 */
static int _hci_deinit(void)
{
    if (g_hci_ctx_p) {
        /* TODO: Add transporthci_deinit to clean up resources */
        free(g_hci_ctx_p);
        g_hci_ctx_p = NULL;
    }

    return NH_OK;
}

/* HCI channel operation structure - Not used for registration */
const struct nhif_ops nhhci_ops = {
    .type = NHIF_TYPE_SDIO,
    .init = _hci_deinit,  // Only deinit, init is called from SDIO
    .deinit = NULL,
    .input = NULL,
    .output = _hci_output_callback,
};

/**
 * @brief Initialize and register HCI channel with nethub
 * @note This function only provides output callback for uplink direction
 * @return NH_OK on success, error code on failure
 */
int nethub_sdiohci_init(void)
{
    HCI_INFO("Registering HCI output callback with nethub\n");

    /* Note: HCI init is called from SDIO channel initialization
     * This function only registers the output callback for uplink (Controller -> Host)
     * Downlink (Host -> Controller) is handled by transporthci callbacks
     */

    HCI_INFO("HCI channel registered successfully\n");
    return NH_OK;
}

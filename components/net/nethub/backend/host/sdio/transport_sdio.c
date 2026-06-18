#include "mr_msg_ctrl.h"
#include "mr_frame_buff_ctrl.h"
#include "mr_netdev.h"

#include "transport_sdio.h"
#include "msg_ctrl_sdio.h"
#include "netdev_wifi.h"
#ifdef CONFIG_MR_TTY
#include "tty_atcmd.h"
#elif defined(CONFIG_MR_VIRTUALCHAN)
#include "nh_vchan_backend.h"
#include "nethub_vchan.h"
#endif
#ifdef CONFIG_MR_HCI
#include "mr_hci.h"
#endif

#define DBG_TAG "NETHUB_SDIO"
#include "log.h"

#ifdef CONFIG_MR_VIRTUALCHAN
static nethub_ctrl_rx_cb_t g_transport_sdio_ctrl_dnld_cb;
static void *g_transport_sdio_ctrl_dnld_arg;

static int transport_sdio_ctrlpath_dnld_dispatch(void *arg, uint8_t *data_buff, uint16_t data_size)
{
    NETHUB_UNUSED(arg);

    if (g_transport_sdio_ctrl_dnld_cb == NULL) {
        return NETHUB_OK;
    }

    return g_transport_sdio_ctrl_dnld_cb(g_transport_sdio_ctrl_dnld_arg, data_buff, data_size);
}
#endif

static int transport_sdio_ctrlpath_upld_send(uint8_t *data_buff, uint32_t data_size)
{
#ifdef CONFIG_MR_TTY
    return tty_atcmd_upld_send(data_buff, data_size);
#elif defined(CONFIG_MR_VIRTUALCHAN)
    return nethub_vchan_at_send(data_buff, (uint16_t)data_size);
#else
    NETHUB_UNUSED(data_buff);
    NETHUB_UNUSED(data_size);
    return NETHUB_ERR_NOT_SUPPORTED;
#endif
}

static int transport_sdio_ctrlpath_dnld_register(nethub_ctrl_rx_cb_t dnld_cb, void *cbpri_arg)
{
#ifdef CONFIG_MR_TTY
    return tty_atcmd_dnld_register(dnld_cb, cbpri_arg);
#elif defined(CONFIG_MR_VIRTUALCHAN)
    g_transport_sdio_ctrl_dnld_cb = dnld_cb;
    g_transport_sdio_ctrl_dnld_arg = cbpri_arg;

    if (dnld_cb == NULL) {
        return nethub_vchan_at_recv_register(NULL, NULL);
    }

    return nethub_vchan_at_recv_register(transport_sdio_ctrlpath_dnld_dispatch, NULL);
#else
    NETHUB_UNUSED(dnld_cb);
    NETHUB_UNUSED(cbpri_arg);
    return NETHUB_ERR_NOT_SUPPORTED;
#endif
}

static int transport_sdio_init(void)
{
    int ret;

    ret = msg_ctrl_sdio_init();
    if (ret < 0 || g_msg_sdio_ctrl == NULL) {
        LOG_E("msg_ctrl_sdio_init failed\r\n");
        return ret < 0 ? ret : -1;
    }

    ret = netdev_wifi_init(g_msg_sdio_ctrl);
    if (ret < 0 || g_netdev_wifi_priv == NULL) {
        LOG_E("netdev_wifi_init failed\r\n");
        return ret < 0 ? ret : -1;
    }

#ifdef CONFIG_MR_HCI
    ret = mr_hci_channel_init(g_msg_sdio_ctrl);
    if (ret < 0) {
        LOG_E("mr_hci_channel_init failed\r\n");
        return ret;
    }
#endif

#ifdef CONFIG_MR_TTY
    ret = tty_atcmd_init(g_msg_sdio_ctrl);
    if (ret < 0) {
        LOG_E("tty_atcmd_init failed\r\n");
        return ret;
    }
#elif defined(CONFIG_MR_VIRTUALCHAN)
    ret = nethub_vchan_backend_init(g_msg_sdio_ctrl);
    if (ret < 0) {
        LOG_E("nethub_vchan_backend_init failed\r\n");
        return ret < 0 ? ret : -1;
    }
#endif

    return NETHUB_OK;
}

static nethub_route_result_t sdio_input_callback(nethub_frame_t *frame, void *arg)
{
    NETHUB_UNUSED(frame);
    NETHUB_UNUSED(arg);
    return NETHUB_ROUTE_CONTINUE;
}

static nethub_route_result_t sdio_output_callback(nethub_frame_t *frame, void *arg)
{
    NETHUB_UNUSED(arg);
    netdev_wifi_upld(frame, NULL);
    return NETHUB_ROUTE_CONTINUE;
}

const struct nhif_ops nhsdio_ops = {
    .type = NETHUB_CHANNEL_SDIO,
    .init = transport_sdio_init,
    .deinit = NULL,
    .input = sdio_input_callback,
    .output = sdio_output_callback,
};

const nh_ctrlpath_ops_t nhsdio_ctrlpath_ops = {
    .upld_send = transport_sdio_ctrlpath_upld_send,
    .dnld_register = transport_sdio_ctrlpath_dnld_register,
};


#include <qcc743_glb.h>
#include <qcc74x_sys.h>
#include <qcc74x_mtd.h>

#if defined (QCC743)
#include <rfparam_adapter.h>
#endif

#if defined (CONFIG_EASYFLASH4)
#include <easyflash.h>
#endif
#include <log.h>
#include <mem.h>
#if CONFIG_SHELL
#include <shell.h>
#endif

#if CONFIG_WIFI6
#include <export/qcc74x_fw_api.h>
#include <wifi_mgmr_ext.h>
#include <wifi_mgmr.h>
#include <lwip/tcpip.h>
#include <lwip/dhcp6.h>
#include <export/coexm.h>
#endif

#if CONFIG_BLUETOOTH
#include "bluetooth.h"
#include "conn.h"
#include "conn_internal.h"
#include "btble_lib_api.h"
#include "hci_driver.h"
#include "hci_core.h"
#endif

#include <lmac154.h>
#include <zb_timer.h>

#if OPENTHREAD_FTD
#include <openthread/dataset_ftd.h>
#endif
#include <openthread/cli.h>
#include <openthread_port.h>

#include "board.h"

static struct qcc74x_device_s *uart0;

extern void __libc_init_array(void);
#if defined(OT_SERIAL_SHELL)
extern void shell_init_with_task(struct qcc74x_device_s *shell);
#endif

#if CONFIG_WIFI6
#define OT_APP_NOTIFY_WIFI_IP_EVENT     0x02000000

#define WIFI_STACK_SIZE  (1536)
#define TASK_PRIORITY_FW (16)

int wifi_start_firmware_task(void)
{
    static TaskHandle_t wifi_fw_task;

    LOG_I("Starting wifi ...\r\n");

    /* enable wifi clock */
    GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY | GLB_AHB_CLOCK_IP_WIFI_MAC_PHY | GLB_AHB_CLOCK_IP_WIFI_PLATFORM);
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI);

    /* Enable wifi irq */
    extern void interrupt0_handler(void);
    qcc74x_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL);
    qcc74x_irq_enable(WIFI_IRQn);

    xTaskCreate(wifi_main, (char *)"fw", WIFI_STACK_SIZE, NULL, TASK_PRIORITY_FW, &wifi_fw_task);

    return 0;
}

void wifi_event_handler(uint32_t code)
{
    switch (code) {
        case CODE_WIFI_ON_INIT_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_INIT_DONE\r\n", __func__);
            wifi_mgmr_init();
        } break;
        case CODE_WIFI_ON_MGMR_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_MGMR_DONE\r\n", __func__);
        } break;
        case CODE_WIFI_ON_SCAN_DONE: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_SCAN_DONE\r\n", __func__);
            wifi_mgmr_sta_scanlist();
        } break;
        case CODE_WIFI_ON_CONNECTED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_CONNECTED\r\n", __func__);

            void mm_sec_keydump();
            mm_sec_keydump();
        } break;
        case CODE_WIFI_ON_GOT_IP: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_GOT_IP\r\n", __func__);
            LOG_I("[SYS] Memory left is %d Bytes\r\n", kfree_size());

            OT_APP_NOTIFY(OT_APP_NOTIFY_WIFI_IP_EVENT);

        } break;
        case CODE_WIFI_ON_DISCONNECT: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_DISCONNECT\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STARTED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STARTED\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STOPPED: {
            LOG_I("[APP] [EVT] %s, CODE_WIFI_ON_AP_STOPPED\r\n", __func__);
        } break;
        case CODE_WIFI_ON_AP_STA_ADD: {
            LOG_I("[APP] [EVT] [AP] [ADD] %lld\r\n", xTaskGetTickCount());
        } break;
        case CODE_WIFI_ON_AP_STA_DEL: {
            LOG_I("[APP] [EVT] [AP] [DEL] %lld\r\n", xTaskGetTickCount());
        } break;
        default: {
            LOG_I("[APP] [EVT] Unknown code %u \r\n", code);
        }
    }
}

void otrAppProcess(ot_system_event_t sevent)
{
    if (OT_APP_NOTIFY_WIFI_IP_EVENT & sevent) {
        wifi_mgmr_sta_autoconnect_enable();
    }
}
#endif

#if CONFIG_BLUETOOTH
static TaskHandle_t ble_start_handle;
struct bt_conn *bleapp_default_conn;
static void ble_connected(struct bt_conn *conn, u8_t err)
{
    if(err || conn->type != BT_CONN_TYPE_LE)
    {
        return;
    }
    if (!bleapp_default_conn) {
        bleapp_default_conn = conn;
    }
}

static void ble_disconnected(struct bt_conn *conn, u8_t reason)
{
    int ret;
    if(conn->type != BT_CONN_TYPE_LE)
    {
        return;
    }
    if (bleapp_default_conn == conn) {
        bleapp_default_conn = NULL;
    }
}
static void ble_conn_param_updated(struct bt_conn *conn, u16_t interval,
			     u16_t latency, u16_t timeout)
{
    if(conn == bleapp_default_conn)
    {
        printf("%s: int 0x%04x lat %d to %d \r\n", __func__, interval, latency, timeout);
    }
}
static struct bt_conn_cb ble_conn_callbacks = {
    .connected  =   ble_connected,
    .disconnected   =   ble_disconnected,
    .le_param_updated = ble_conn_param_updated,
};

void bt_enable_cb(int err)
{
    if (!err) {
        bt_addr_le_t bt_addr;
        bt_get_local_public_address(&bt_addr);
        printf("BD_ADDR:(MSB)%02x:%02x:%02x:%02x:%02x:%02x(LSB) \r\n",
                bt_addr.a.val[5], bt_addr.a.val[4], bt_addr.a.val[3], bt_addr.a.val[2], bt_addr.a.val[1], bt_addr.a.val[0]);
        bt_conn_cb_register(&ble_conn_callbacks);
        ble_cli_register();
    }
}

static void ble_start_task(void *pvParameters)
{
    btble_controller_init(configMAX_PRIORITIES - 1);
    hci_driver_init();
    bt_enable(bt_enable_cb);
    vTaskDelete(NULL);
}
#endif

static void ot_stateChangedCallback(otChangedFlags aFlags, void *aContext)
{
    char string[OT_IP6_ADDRESS_STRING_SIZE];

    if (OT_CHANGED_THREAD_ROLE & aFlags) {
        otDeviceRole role = otThreadGetDeviceRole(otrGetInstance());
        const otNetifAddress *unicastAddrs = otIp6GetUnicastAddresses(otrGetInstance());

        APP_PRINT ("state_role_changed %ld %s, partition id %d\r\n", zb_timer_get_current_time(),
                otThreadDeviceRoleToString(role), otThreadGetPartitionId(otrGetInstance()));

        for (const otNetifAddress *addr = unicastAddrs; addr; addr = addr->mNext) {
            otIp6AddressToString(&addr->mAddress, string, sizeof(string));
            APP_PRINT ("%s\r\n", string);
        }
    }

    if (OT_CHANGED_IP6_ADDRESS_ADDED & aFlags) {
        otDeviceRole role = otThreadGetDeviceRole(otrGetInstance());
        const otNetifAddress *unicastAddrs = otIp6GetUnicastAddresses(otrGetInstance());

        APP_PRINT ("state_address_changed %ld %s, partition id %d\r\n", zb_timer_get_current_time(),
                otThreadDeviceRoleToString(role), otThreadGetPartitionId(otrGetInstance()));

        for (const otNetifAddress *addr = unicastAddrs; addr; addr = addr->mNext) {
            otIp6AddressToString(&addr->mAddress, string, sizeof(string));
            APP_PRINT ("%s\r\n", string);
        }
    }

    if (OT_CHANGED_THREAD_PARTITION_ID & aFlags) {
        APP_PRINT ("st_part %ld %s, partition id %d\r\n", zb_timer_get_current_time(),
                otThreadDeviceRoleToString(otThreadGetDeviceRole(otrGetInstance())), otThreadGetPartitionId(otrGetInstance()));
    }
}

void otrInitUser(otInstance * instance)
{
    otAppCliInit((otInstance * )instance);

    if (false == otIp6IsEnabled(otrGetInstance())) {
        otIp6SetEnabled(otrGetInstance(), true);
        otThreadSetEnabled(otrGetInstance(), true);
    }

    otSetStateChangedCallback(otrGetInstance(), ot_stateChangedCallback, NULL);

#if CONFIG_WIFI6
    int iret = wifi_mgmr_sta_coex_duty_set(30);
    wifi_mgmr_sta_coex_enable();
    if (iret != 0) {
        LOG_E("Failed to set coexistence duty cycle with error code: %d\r\n", iret);
        return;
    }
#endif
}

void app_start_otr(void)
{
    otRadio_opt_t opt;

    opt.byte = 0;

    opt.bf.isCoexEnable = true;
    opt.bf.isFtd = true;
    #if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
    opt.bf.isLinkMetricEnable = true;
    #endif
    #if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    opt.bf.isCSLReceiverEnable = true;
    #endif
    #if OPENTHREAD_CONFIG_TIME_SYNC_ENABLE
    opt.bf.isTimeSyncEnable = true;
    #endif

    otrStart(opt);
}

void vApplicationTickHook( void )
{
#ifdef QCC743
    lmac154_monitor();
#endif
#if CONFIG_LMAC154_LOG
    lmac154_logs_output();
#endif
}

int main(void)
{
#if !defined(QCC74x_undefL)
    qcc74x_sys_rstinfo_init();
#endif

    board_init();

    qcc74x_mtd_init();
#if CONFIG_BLUETOOTH
    easyflash_init();
#endif
    configASSERT((configMAX_PRIORITIES > 4));

#if defined(QCC743)
    if (0 != rfparam_init(0, NULL, 0)) {
        printf("PHY RF init failed!\r\n");
        return 0;
    }
#endif

    __libc_init_array();

#if CONFIG_LMAC154_LOG
    lmac154_log_init();
#endif

    uart0 = qcc74x_device_get_by_name("uart0");
#if defined(OT_SERIAL_SHELL)
    shell_init_with_task(uart0);
#elif defined (OT_SERIAL_UART)
    ot_uart_init(uart0);
#else
    #error "No serial interface specified."
#endif

#if CONFIG_WIFI6
    tcpip_init(NULL, NULL);
    wifi_start_firmware_task();
#endif
#if CONFIG_BLUETOOTH
    xTaskCreate(ble_start_task, (char *)"ble_start", 1024, NULL, 15, &ble_start_handle);
#endif

    app_start_otr();

    puts("[OS] Starting OS Scheduler...\r\n");
    vTaskStartScheduler();

    while (1) {
    }
}

#if CONFIG_WIFI6 || CONFIG_BLUETOOTH
int cmd_app_coex_info(int argc, char **argv)
{
    bool is_rx_on_after_rx_abort_enable;
    uint32_t tx_priority;
    uint32_t rx_unicast_priority;
    uint32_t rx_sfd_priority;
    uint32_t rx_idle_priority;

#if CONFIG_WIFI6
    coex_protect_dump();
    printf ("WLAN coex duty cycle: %d%\r\n", wifi_mgmr_sta_coex_duty_get());
#endif

    lmac154_getCoexPriority(&is_rx_on_after_rx_abort_enable, &tx_priority, &rx_unicast_priority, &rx_sfd_priority, &rx_idle_priority);

#if CONFIG_BLUETOOTH
    uint32_t connect_ind_priority = btble_controller_get_event_priority(BLE_EVENT_CONNECT_IND_TX_RX);
    uint32_t llcp_message_priority = btble_controller_get_event_priority(BLE_EVENT_LLCP_MESSAGE);
    uint32_t data_channel_tx_priority = btble_controller_get_event_priority(BLE_EVENT_DATA_CHANNEL_TX);
    uint32_t initiating_scanning_priority = btble_controller_get_event_priority(BLE_EVENT_INITIATING_SCANNING);
    uint32_t active_scanning_priority = btble_controller_get_event_priority(BLE_EVENT_ACTIVE_SCANNING);
    uint32_t connectable_adv_priority = btble_controller_get_event_priority(BLE_EVENT_CONNECTABLE_ADV);
    uint32_t non_connectable_adv_priority = btble_controller_get_event_priority(BLE_EVENT_NON_CONNECTABLE_ADV);
    uint32_t passive_scanning_priority = btble_controller_get_event_priority(BLE_EVENT_PASSIVE_SCANNING);

    printf ("BLE coex settings: connect_ind %d, llcp_msg %d, data_channel_tx %d, init_scan %d, "
            "active_scan %d, connectable_adv %d, non_connectable_adv %d, passive_scan %d\r\n",
        connect_ind_priority, llcp_message_priority, data_channel_tx_priority, initiating_scanning_priority,
        active_scanning_priority, connectable_adv_priority, non_connectable_adv_priority, passive_scanning_priority);
#endif

    printf ("Thread coex settings: tx %d, rx_unicast %d, rx_sfd %d, rx_idle %d\r\n",
        tx_priority, rx_unicast_priority, rx_sfd_priority, rx_idle_priority);
    return 0;
}
SHELL_CMD_EXPORT_ALIAS(cmd_app_coex_info, app_coex_info, print out coex information);
#endif


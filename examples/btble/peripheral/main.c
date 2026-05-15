#include "shell.h"
#include <FreeRTOS.h>
#include "task.h"
#include "board.h"

#include "bluetooth.h"
#include "conn.h"
#include "conn_internal.h"
#if defined(QCC74x_undef) || defined(QCC74x_undef)
#include "ble_lib_api.h"
#elif defined(QCC743)
#include "btble_lib_api.h"
#include "qcc743_glb.h"
#include "rfparam_adapter.h"
#elif defined(QCC74x_undef)
#include "btble_lib_api.h"
#include "qcc74x_undef_glb.h"
#endif

#include "ble_tp_svc.h"
#include "hci_driver.h"
#include "hci_core.h"
#include "bt_log.h"

#include "qcc74x_mtd.h"
#include "easyflash.h"
#if defined(CONFIG_BT_OAD_SERVER)
#include "oad_main.h"
#endif

#if defined(CONFIG_BT_BAS_SERVER)
#include "bas.h"
#endif

#if defined(CONFIG_BT_DIS_SERVER)
#include "dis.h"
#endif

#if defined(CONFIG_BT_IAS_SERVER)
#include "ias.h"
#endif

#if defined(CONFIG_BT_CONN)
static struct bt_conn *default_conn = NULL;
#endif

static struct qcc74x_device_s *uart0;

extern void shell_init_with_task(struct qcc74x_device_s *shell);

static void ble_connected(struct bt_conn *conn, u8_t err)
{
	
    if(err || conn->type != BT_CONN_TYPE_LE)
    {
        return;
    }
    
    default_conn = conn;

    printf("%s",__func__);
}

static void ble_disconnected(struct bt_conn *conn, u8_t reason)
{ 
    int ret;

    if(conn->type != BT_CONN_TYPE_LE)
    {
        return;
    }
    default_conn = NULL;
    BT_WARN("%s",__func__);

    // enable adv
    ret = set_adv_enable(true);
    if(ret) {
        BT_WARN("Restart adv fail. \r\n");
    }
}

#if defined(CONFIG_BT_BAS_SERVER)
void set_bat_level (int argc, char **argv)
{
	u8_t level;
	
	if (!default_conn) {
		BT_WARN("Not connected\r\n");
		return;
	}
	
	get_uint8_from_string(&argv[1], &level);
	bt_gatt_bas_set_battery_level (default_conn, level);
   
}
SHELL_CMD_EXPORT_ALIAS(set_bat_level, set_bat_level, cmd set_bat_level);
#endif

static struct bt_conn_cb ble_conn_callbacks = {
	.connected	=   ble_connected,
	.disconnected	=   ble_disconnected,
};

static void ble_start_adv(void)
{
    struct bt_le_adv_param param;
    int err = -1;
    struct bt_data adv_data[1] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR | BT_LE_AD_GENERAL)
    };
    struct bt_data adv_rsp[1] = {
        BT_DATA_BYTES(BT_DATA_MANUFACTURER_DATA, "QCC743")
    };

    memset(&param, 0, sizeof(param));
    // Set advertise interval
    param.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
    param.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
    /*Get adv type, 0:adv_ind,  1:adv_scan_ind, 2:adv_nonconn_ind 3: adv_direct_ind*/
    param.options = (BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME | BT_LE_ADV_OPT_ONE_TIME); 

    err = bt_le_adv_start(&param, adv_data, ARRAY_SIZE(adv_data), adv_rsp, ARRAY_SIZE(adv_rsp));
    if(err){
        printf("Failed to start advertising (err %d) \r\n", err);
    }
    printf("Start advertising success.\r\n");
}

#if defined(CONFIG_BT_OAD_SERVER)
bool ble_check_oad(u32_t cur_file_ver, u32_t new_file_ver)
{
    return true;
}
#endif

#if defined (CONFIG_BT_IAS_SERVER)

static void ias_recv(struct bt_conn *conn, void *buf, u8_t len)
{
    struct net_buf_simple data;
    net_buf_simple_init_with_data(&data, (void *)buf, len);
    u8_t alert_val = net_buf_simple_pull_u8(&data);
	if (alert_val < 0x00 || alert_val > 0x02) {
		BT_WARN("Unkown IAS Level");
        return;
	}
    if(alert_val == 0x00 )
        BT_WARN("No alert");
    else if (alert_val == 0x01 )
        BT_WARN("Mild alert");
    else 
        BT_WARN("High alert");
}
#endif
void bt_enable_cb(int err)
{
    if (!err) {
        bt_addr_le_t bt_addr;
        bt_get_local_public_address(&bt_addr);
        printf("BD_ADDR:(MSB)%02x:%02x:%02x:%02x:%02x:%02x(LSB) \r\n",
            bt_addr.a.val[5], bt_addr.a.val[4], bt_addr.a.val[3], bt_addr.a.val[2], bt_addr.a.val[1], bt_addr.a.val[0]);

        bt_conn_cb_register(&ble_conn_callbacks);
        ble_tp_init();
#if defined(CONFIG_BT_OAD_SERVER)
        oad_service_enable(ble_check_oad);
        bt_oad_enable_data_len_exchange(false);
#endif

#if defined(CONFIG_BT_BAS_SERVER)
        bas_init();
#endif

#if defined(CONFIG_BT_IAS_SERVER)
        ias_init();
        ias_register_recv_callback(ias_recv);
#endif

#if defined(CONFIG_BT_DIS_SERVER)
        dis_init(0x10, 0x12, 0xab, 0xff);
#endif


        // start advertising
        ble_start_adv();
    }
}

static TaskHandle_t app_start_handle;

static void app_start_task(void *pvParameters)
{
    // Initialize BLE controller
    #if defined(QCC74x_undef) || defined(QCC74x_undef)
    ble_controller_init(configMAX_PRIORITIES - 1);
    #else
    btble_controller_init(configMAX_PRIORITIES - 1);
    #endif
    // Initialize BLE Host stack
    hci_driver_init();
    bt_enable(bt_enable_cb);

    vTaskDelete(NULL);
}

int main(void)
{
    board_init();

    configASSERT((configMAX_PRIORITIES > 4));

    uart0 = qcc74x_device_get_by_name("uart0");
    shell_init_with_task(uart0);

    qcc74x_mtd_init();
    /* ble stack need easyflash kv */
    easyflash_init();

#if defined(QCC743)
    /* Init rf */
    if (0 != rfparam_init(0, NULL, 0)) {
        printf("PHY RF init failed!\r\n");
        return 0;
    }
#endif

    xTaskCreate(app_start_task, (char *)"app_start", 1024, NULL, configMAX_PRIORITIES - 2, &app_start_handle);

    vTaskStartScheduler();

    while (1) {
    }
}
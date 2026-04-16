#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include <sys/errno.h>
#include "btble_lib_api.h"
#include "bluetooth.h"
#include "conn.h"
#include "conn_internal.h"
#include "hci_driver.h"
#include "hci_core.h"
#include "hci_host.h"
#include "bt_log.h"
#include "btble_lib_api.h"
#include "bas.h"
#include "gatt.h"
#include "ble_rc_hog.h"
#include "ble_rc_app.h"
#if defined(CONFIG_BT_OAD_SERVER)
#include "oad_main.h"
#include "oad_service.h"
#endif
#if defined(CONFIG_SHELL)
#include "shell.h"
#endif

//#define BLE_RC_PDS_SECTION_ENABLE
volatile bool ir_tx_start = false;
volatile bool voice_start = false;
volatile bool cont_start = false;//send release key value after key is released.
volatile bool cont_release = false;
volatile bool wait_for_unpair = false;
volatile u8_t rc_battery_level = 80;
volatile u8_t bonded_device_cnt = 0;
bool pending_key = false;

struct k_fifo ble_rc_key_scan_queue;
static struct bt_gatt_exchange_params exchange_params;
struct bt_conn *rc_default_conn;
struct hids_remote_key *m_key_usage;

k_timer_t ble_rc_start_pairing_timer;//restart adv
k_timer_t ble_rc_stop_adv_timer;//stop adv

extern struct hids_remote_key remote_kbd_map_tab[];
extern uint8_t KEY_CH_INS[8]; //Keyboard Pageup
extern uint8_t KEY_CH_DES[8]; //Keyboard Pagedown
extern uint8_t KEY_LEFT[8]; //keyboard RightArrow
extern uint8_t KEY_RIGHT[8]; //keyboard LeftArrow
extern uint8_t KEY_DOWN[8]; //keyboard DownArrow
extern uint8_t KEY_UP[8]; //keyboard UpArrow
extern uint8_t KEY_MENU[8]; //keyboard Application
extern uint8_t KEY_PWR[8]; //keyboard power
extern uint8_t KEY_PICK[2]; //Menu Pick  
extern uint8_t KEY_MUTE[2]; //MUTE
extern uint8_t KEY_VOL_INS[2]; //Volume Increment
extern uint8_t KEY_VOL_DES[2]; //Volume Decrement
extern uint8_t KEY_HOME[2]; //AC Home
extern uint8_t KEY_BACK[2]; //AC Back

typedef struct{
    uint8_t evt_type;
    bool press;
    bool auto_release;
    struct hids_remote_key *key_usage;
}ble_rc_notify_key_info_t;
ble_rc_notify_key_info_t pending_notify_key_info;
static int ble_rc_start_adv(void);
static void ble_rc_key_notify_process(u8_t hid_page, u8_t *hid_usage, bool press, bool auto_release);
static void ble_rc_key_notify(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv);

void ble_rc_create_stop_adv_timer(void);
#if defined(CONFIG_SHELL)
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_ch_ins, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_ch_des, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_right, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_left, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_down, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_up, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_menu, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_pwr, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_pick, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_mute, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_home, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_vol_ins, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_vol_des, );
SHELL_CMD_EXPORT_ALIAS(ble_rc_key_notify, ble_key_back, );
#endif

static void ble_rc_data_len_extend(struct bt_conn *conn)
{
    u16_t tx_octets = 0x00fb;
    u16_t tx_time = 0x0848;
    int ret = -1;
    
    if(conn == NULL)
        return;
    //set data length after connected.
    ret = bt_le_set_data_len(conn, tx_octets, tx_time);
    if(!ret)
    {
        printf("ble rc set data length success.");
    }
    else
    {
        printf("ble rc set data length failure, err: %d\n", ret);
    }
}

static void ble_rc_exchange_func(struct bt_conn *conn, u8_t err,
    struct bt_gatt_exchange_params *params)
{ 
    printf("Exchange %s MTU Size =%d \r\n", err == 0U ? "successful" : "failed",bt_gatt_get_mtu(conn));
    if(!err)
        ble_rc_data_len_extend(rc_default_conn);
}

static void ble_rc_gatt_exchange_mtu(void)
{    
    exchange_params.func = ble_rc_exchange_func;
    bt_gatt_exchange_mtu(rc_default_conn, &exchange_params);
}

void ble_rc_create_adv_timer(void)
{
    k_timer_start(&ble_rc_start_pairing_timer, 2 * 1000);
}

static void ble_rc_stop_adv_timer_cb(void *timer)
{
    if(rc_default_conn == NULL && pending_notify_key_info.evt_type != RC_KYS_INVALID_EVT)
         pending_notify_key_info.evt_type = RC_KYS_INVALID_EVT;

    printf("stop adv\r\n");
    bt_le_adv_stop();
}

static void ble_rc_connected(struct bt_conn *conn, u8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    if(ble_rc_stop_adv_timer.timer.hdl && k_timer_is_active(&ble_rc_stop_adv_timer))
    {
        k_timer_stop(&ble_rc_stop_adv_timer);
        ble_rc_stop_adv_timer_cb(NULL);
    }
    
    if (err) {
        printf("Failed to connect to %s (%u) \r\n", addr, err);
        pending_notify_key_info.evt_type = RC_KYS_INVALID_EVT;
        //if high duty cycle directed adv timeout, start connectionable adv
        if(err == BT_HCI_ERR_ADV_TIMEOUT)
        {
            ble_rc_create_adv_timer();
        }
        return;
    }

    printf("Connected: %s,int 0x%04x lat %d to %d \r\n", addr,conn->le.interval,conn->le.latency,conn->le.timeout);

    if (!rc_default_conn) {
        rc_default_conn = conn;
    }

    ble_rc_gatt_exchange_mtu();
}

static void ble_rc_disconnected(struct bt_conn *conn, u8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printf("Disconnected: %s (reason %u) \r\n", addr, reason);

    if (rc_default_conn == conn) {
        rc_default_conn = NULL;
    }
}

static void ble_rc_auth_cancel(struct bt_conn *conn)
{  
    if (rc_default_conn) {
        conn = rc_default_conn;
    }else {
        conn = NULL;
    }
    
    if (!conn) {
        printf("Not connected\r\n");
        return;
    }
    
    bt_conn_auth_cancel(conn);
}

static void ble_rc_auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));    
    printf("passkey_str is: %06u\r\n", passkey);
}

static void ble_rc_auth_passkey_entry(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printf("Enter passkey for %s\r\n", addr);
}

static void  ble_rc_auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printf("Confirm passkey for %s: %06u\r\n", addr, passkey);
    bt_conn_auth_passkey_confirm(rc_default_conn);
}

static void ble_rc_auth_pairing_confirm(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];
    
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printf("Confirm pairing for %s\r\n", addr);
    bt_conn_auth_pairing_confirm(conn);
}

static void ble_rc_auth_pairing_complete(struct bt_conn *conn, bool bonded)
{
    printf("auth_pairing_complete\r\n");
}

static void ble_rc_conn_param_updated(struct bt_conn *conn, u16_t interval,
			     u16_t latency, u16_t timeout)
{
    if(conn == rc_default_conn)
    {
        printf("%s: int 0x%04x lat %d to %d \r\n", __func__, interval, latency, timeout);
    }
}
static void ble_rc_auth_pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printf("Pairing failed with %s\r\n", addr);
}

static void ble_rc_identity_resolved(struct bt_conn *conn, const bt_addr_le_t *rpa,
			      const bt_addr_le_t *identity)
{
    char addr_identity[BT_ADDR_LE_STR_LEN];
    char addr_rpa[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(identity, addr_identity, sizeof(addr_identity));
    bt_addr_le_to_str(rpa, addr_rpa, sizeof(addr_rpa));

    printf("Identity resolved %s -> %s \r\n", addr_rpa, addr_identity);
}

static void ble_rc_security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printf("Security changed: %s level %u \r\n", addr, level);    
}

void ble_rc_check_pending_evt(void)
{
    if(pending_notify_key_info.evt_type != RC_KYS_INVALID_EVT)
    {
        if(pending_notify_key_info.evt_type == RC_KYS_NOTIFY)
        {
            pending_notify_key_info.auto_release = true;
            pending_notify_key_info.press = true;
        }
        printf("check,%p\r\n", pending_notify_key_info.key_usage);
        k_fifo_put_from_isr(&ble_rc_key_scan_queue, (void *)&pending_notify_key_info);
    }
}

void ble_rc_foreach_bond_info_cb(const struct bt_bond_info *info, void *user_data)
{
    char addr[BT_ADDR_LE_STR_LEN];
    if(user_data)
        (*(u8_t *)user_data)++;

    bt_addr_le_to_str(&info->addr, addr, sizeof(addr));
    printf("bonded device: %s\r\n", addr);
}

static struct bt_conn_cb ble_rc_conn_callbacks = {
    .connected = ble_rc_connected,
    .disconnected = ble_rc_disconnected,
    .le_param_updated = ble_rc_conn_param_updated,
    .identity_resolved = ble_rc_identity_resolved,
    .security_changed = ble_rc_security_changed,
};

static struct bt_conn_auth_cb ble_rc_auth_cb_display = {
	.passkey_display = NULL,//ble_rc_auth_passkey_display,
	.passkey_entry = NULL,//ble_rc_auth_passkey_entry,
	.passkey_confirm = NULL,//ble_rc_auth_passkey_confirm,
	.cancel = ble_rc_auth_cancel,
	.pairing_confirm = ble_rc_auth_pairing_confirm,
	.pairing_failed = ble_rc_auth_pairing_failed,
	.pairing_complete = ble_rc_auth_pairing_complete,
};

static void ble_rc_key_notify_process(u8_t hid_page, u8_t *hid_usage, bool press, bool auto_release)
{
    int err = 0;

    if(!rc_default_conn){
        printf("Not connected\r\n");
        return;
    }

    err = bt_hog_notify(rc_default_conn, hid_page, hid_usage, press);

    if(auto_release && err == 0)
    {
        err = bt_hog_notify(rc_default_conn, hid_page, hid_usage, 0);
    }

    if(err)
    {
        printf("Notification sent failed\r\n");
    }
    else
    {    
        printf("Notification sent successfully\r\n");
    }
}

static void ble_rc_key_notify(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    u8_t *hid_usage = NULL;
    u8_t hid_page = HID_PAGE_KBD;
    bool auto_release = true;
    bool press = true;
    u8_t evt_type = 0;
    if(0==strcmp("ble_key_ch_ins",argv[0]))
    {
        hid_usage = KEY_CH_INS;
    }
    else if(0==strcmp("ble_key_ch_des",argv[0]))
    {
        hid_usage = KEY_CH_DES;
    }
    else if(0==strcmp("ble_key_right",argv[0]))
    {
        hid_usage = KEY_RIGHT;
    }
    else if(0==strcmp("ble_key_left",argv[0]))
    {
        hid_usage = KEY_LEFT;
    }
    else if(0==strcmp("ble_key_down",argv[0]))
    {
        hid_usage = KEY_DOWN;
    }
    else if(0==strcmp("ble_key_up",argv[0]))
    {
        hid_usage = KEY_UP;
    }
    else if(0==strcmp("ble_key_menu",argv[0]))
    {
        hid_usage = KEY_MENU;
    }
    else if(0==strcmp("ble_key_pwr",argv[0]))
    {
        hid_usage = KEY_PWR;
    }
    else if(0==strcmp("ble_key_pick",argv[0]))
    {
        hid_page = HID_PAGE_CONS;
        hid_usage = KEY_PICK;
    }
    else if(0==strcmp("ble_key_mute",argv[0]))
    {
        hid_page = HID_PAGE_CONS;
        hid_usage = KEY_MUTE;
    }
    else if(0==strcmp("ble_key_home",argv[0]))
    {
        hid_page = HID_PAGE_CONS;
        hid_usage = KEY_HOME;
    }
    else if(0==strcmp("ble_key_back",argv[0]))
    {
        hid_page = HID_PAGE_CONS;
        hid_usage = KEY_BACK;
    }
    else if(0==strcmp("ble_key_vol_ins",argv[0]))
    {
        hid_page = HID_PAGE_CONS;
        hid_usage = KEY_VOL_INS;
    }
    else if(0==strcmp("ble_key_vol_des",argv[0]))
    {
        hid_page = HID_PAGE_CONS;
        hid_usage = KEY_VOL_DES;
    }
    else if(0==strcmp("ble_key_voice_start",argv[0]))
    {
        voice_start = true;
        evt_type = RC_KYS_VOICE_START;
        printf("put RC_KYS_VOICE_START\r\n");
        k_fifo_put(&ble_rc_key_scan_queue, &evt_type);
        return;
    }
    else if(0==strcmp("ble_key_voice_stop",argv[0]))
    {
       if(voice_start)
        {
            voice_start = false;
            evt_type = RC_KYS_VOICE_STOP;
            k_fifo_put(&ble_rc_key_scan_queue, &evt_type); 
        }
       return;
    }
    else
    {
        printf("Faild to find hid usage");
        return;
    }
    
    ble_rc_key_notify_process(hid_page, hid_usage, press, auto_release);
}

static void ble_rc_start_adv_timer_cb(void *timer)
{
    int err = ble_rc_start_adv();
    if(err)
        printf("fail to start adv,err=%d\r\n", err);
    else
        printf("start undirect connectable adv succesfully\r\n");
    ble_rc_create_stop_adv_timer();
}

void ble_rc_start_pairing(void)
{
    bt_foreach_bond(0, ble_rc_foreach_bond_info_cb, (void *)&bonded_device_cnt);
    if(bonded_device_cnt)
    {
        printf("unpair\r\n");
        bt_unpair(0, NULL);
        ble_rc_create_adv_timer();
    }
    else
    {
        printf("start adv\r\n");
        ble_rc_start_adv();
        ble_rc_create_stop_adv_timer();
    }        
}

void ble_rc_create_stop_adv_timer(void)
{
    if(ble_rc_stop_adv_timer.timer.hdl)
        k_timer_start(&ble_rc_stop_adv_timer, 60 * 1000);
    else
        printf("Stop adv timer fail\n");
}

int ble_rc_start_high_duty_cycle_directed_adv(bt_addr_le_t *peer_addr)
{
    struct bt_le_adv_param adv_param = {
       //options:3, connectable undirected, adv one time
       .options = 3, \
       .interval_min = BT_GAP_ADV_FAST_INT_MIN_3, \
       .interval_max = BT_GAP_ADV_FAST_INT_MAX_3, \
     };

    if(bt_conn_create_slave_le((const bt_addr_le_t *)peer_addr, &adv_param))
        return 0;
    else
        return -1;
}

void ble_rc_get_bonded_addr(const struct bt_bond_info *info, void *user_data)
{
    if(user_data)
    {
        bt_addr_le_copy(user_data, &info->addr);
    }
}

static struct hids_remote_key *key_usage[4 /* row */][4 /* col */] = {
    {&remote_kbd_map_tab[4], &remote_kbd_map_tab[0],  &remote_kbd_map_tab[1], &remote_kbd_map_tab[2]  },
    {&remote_kbd_map_tab[3], NULL ,                   &remote_kbd_map_tab[5], &remote_kbd_map_tab[6]  },
    {&remote_kbd_map_tab[7], &remote_kbd_map_tab[8],  &remote_kbd_map_tab[9], &remote_kbd_map_tab[10] },
    {&remote_kbd_map_tab[11], &remote_kbd_map_tab[12],&remote_kbd_map_tab[13],NULL                    },
};

int ble_rc_start_adv(void)
{    
    struct bt_le_adv_param adv_param = {
        //options:3, connectable undirected, adv one time
        .options = 3, \
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_3, \
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_3, \
    };

    char *adv_name = BLE_RC_NAME;
    u8_t data[1] = {(BT_LE_AD_LIMITED | BT_LE_AD_NO_BREDR)};
    u8_t data_uuid[4] = {0x12, 0x18, 0x0F, 0x18};//0x1812,0x180f
    u8_t data_appearance[2] = {0x80, 0x01};//{0xc1, 0x03};//0x0180 {0x80, 0x01};
    //u8_t data_manuf_data[15] = {0xe0, 0x00, 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
    struct bt_data adv_data[] = {
            BT_DATA(BT_DATA_FLAGS, data, 1),
            BT_DATA(BT_DATA_GAP_APPEARANCE, data_appearance, sizeof(data_appearance)),
            BT_DATA(BT_DATA_UUID16_SOME, data_uuid, sizeof(data_uuid)),
            BT_DATA(BT_DATA_NAME_COMPLETE, adv_name, strlen(adv_name)),
        };

    return bt_le_adv_start(&adv_param, adv_data, ARRAY_SIZE(adv_data), NULL, 0);
}

#if defined(CONFIG_BT_OAD_SERVER)
bool ble_rc_check_oad(u32_t cur_file_ver, u32_t new_file_ver)
{
    return true;
}
#endif

void bt_enable_cb(int err)
{
    if (!err) {
        bt_addr_le_t bt_addr;
        bt_get_local_public_address(&bt_addr);
        printf("BD_ADDR:(MSB)%02x:%02x:%02x:%02x:%02x:%02x(LSB) \n",
            bt_addr.a.val[5], bt_addr.a.val[4], bt_addr.a.val[3], bt_addr.a.val[2], bt_addr.a.val[1], bt_addr.a.val[0]);
        bt_set_name(BLE_RC_NAME);
        bt_gap_set_local_device_appearance(BLE_RC_DEVICE_APPEARANCE);
        hog_init();

        k_timer_init(&ble_rc_start_pairing_timer, ble_rc_start_adv_timer_cb, NULL);
        k_timer_init(&ble_rc_stop_adv_timer, ble_rc_stop_adv_timer_cb, NULL);
        
        bt_conn_auth_cb_register(&ble_rc_auth_cb_display);
        bt_conn_cb_register(&ble_rc_conn_callbacks);

        #if defined(CONFIG_BT_OAD_SERVER)
        oad_service_enable(ble_rc_check_oad);
        bt_oad_enable_data_len_exchange(false);
        #endif

        //If there is no bonded device, ble_rc_foreach_bond_info_cb will not be called.
        //If there is N bonded devices, ble_rc_foreach_bond_info_cb will be called N times.
        bonded_device_cnt = 0;
        bt_foreach_bond(0, ble_rc_foreach_bond_info_cb, (void *)&bonded_device_cnt);
        printf("%d peer device(s) bonded\r\n", bonded_device_cnt);
        if(bonded_device_cnt)
        {
            bt_addr_le_t peer_addr;
            bt_addr_le_copy(&peer_addr, BT_ADDR_LE_NONE);
            bt_foreach_bond(0, ble_rc_get_bonded_addr, &peer_addr);
            if(bt_addr_le_cmp(&peer_addr, BT_ADDR_LE_NONE))
            {
                err = ble_rc_start_high_duty_cycle_directed_adv(&peer_addr);
                printf("start directed adv err=%d\r\n", err);
            }
        }
        else
        {
            err = ble_rc_start_adv();
            if(err == 0)
            {
                printf("ble advertising is started\r\n");
            }
            else
            {
                printf("ble advertising failed, err %d\r\n", err);
            }
           
            ble_rc_create_stop_adv_timer();
        }
    }
}

void ble_stack_start(void)
{
    pending_notify_key_info.evt_type = RC_KYS_INVALID_EVT;
    // Initialize BLE controller
    btble_controller_init(configMAX_PRIORITIES - 1);
    // Initialize BLE Host stack
    hci_driver_init();
    bt_enable(bt_enable_cb);
}

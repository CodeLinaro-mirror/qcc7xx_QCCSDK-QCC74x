/**
 * @file shell_commands.c
 * @brief UART-based shell commands for WiFi and BT RCP control
 */

#include "shell_commands.h"
#include "main.h"
#include "rcp_unit_test.h"
#include "stm32h7rsxx_hal_uart_ex.h"
// #include "bt_host.h"
#include "app_ble.h"
#include "ble_hal_aci.h"
#include "ble_hci_le.h"
#include "rcp_api.h"
#include "FreeRTOS.h"
#include "main.h"
#include "stm32h7rsxx_hal_uart_ex.h"
#include "stream_buffer.h"
#include "task.h"
#include <bt_hci.h>
#include <cmsis_os2.h>
#include <lwip/ip_addr.h>
#include <rcp_data_test.h>
#include <rcp_interface.h>
#include <rcp_stat.h>
#include <rcp_bus_sdio_master.h>
#include <rcp_mcu_capability.h>
#include <rcp_user_config.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rcp_tty.h"
#include "app_ble.h"

void iperf_start(uint8_t is_client, uint8_t is_udp, const char *ip);
TaskHandle_t fhost_iperf_msg_handle_get(void);
int fhost_iperf_sigkill_handler(TaskHandle_t iperf_handle);

/* External UART handle */
extern UART_HandleTypeDef huart3;

#define TESTBUFF_SIZE 1600
/* Shell buffer */
#define SHELL_BUF_SIZE 256
#define SHELL_RX_STREAM_SIZE 512
#define SHELL_UART_RX_CHUNK_SIZE 32
static char shell_buffer[SHELL_BUF_SIZE];
static uint16_t shell_idx = 0;
static uint8_t shell_uart_rx_chunk[SHELL_UART_RX_CHUNK_SIZE];
static uint8_t shell_rx_stream_storage[SHELL_RX_STREAM_SIZE];
static StaticStreamBuffer_t shell_rx_stream_struct;
static StreamBufferHandle_t shell_rx_stream;
static volatile uint32_t shell_rx_dropped;
static bool shell_last_was_cr;

/* Shell task */
osThreadId_t shell_task_handle;
const osThreadAttr_t shell_task_attr = {
    .name = "shell_task",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityLow,
};

/* Function prototypes */
static void shell_task(void *argument);
static void shell_process_command(char *cmd);
static void rcp_test_task(void *argument);
static void shell_handle_char(uint8_t ch);
static void shell_uart_rx_start(void);

/* iperf commands */
static void cmd_iperf_tcp_tx(int argc, char **argv);
static void cmd_iperf_tcp_rx(int argc, char **argv);
static void cmd_iperf_udp_tx(int argc, char **argv);
static void cmd_iperf_udp_rx(int argc, char **argv);
static void cmd_iperf_stop(int argc, char **argv);

/* BT commands */
static bool hci_initialized = false;
static void hci_cli_init(void);
static void cmd_hci_reset(int argc, char **argv);
static void cmd_hci_set_event_mask(int argc, char **argv);
static void cmd_hci_le_set_event_mask(int argc, char **argv);
static void cmd_hci_read_version(int argc, char **argv);
static void cmd_hci_send(int argc, char **argv);
static void cmd_bt_adv_start(int argc, char **argv);
static void cmd_bt_adv_stop(int argc, char **argv);
static void cmd_bt_connect(int argc, char **argv);
static void cmd_bt_add_service(int argc, char **argv);
/* ACI HAL vendor commands (OGF=0x3F) */
static void cmd_bt_set_tx_power(int argc, char **argv);
static void cmd_bt_read_rssi(int argc, char **argv);
static void cmd_bt_link_status(int argc, char **argv);
static void cmd_bt_radio_activity_mask(int argc, char **argv);
static void cmd_bt_periph_latency(int argc, char **argv);
static void cmd_bt_read_config(int argc, char **argv);
static void cmd_bt_set_bd_addr(int argc, char **argv);
static void cmd_bt_tone_start(int argc, char **argv);
static void cmd_bt_tone_stop(int argc, char **argv);
static void cmd_bt_tx_test_pkt_num(int argc, char **argv);
static void cmd_bt_continuous_tx(int argc, char **argv);
static void cmd_bt_le_tx_test(int argc, char **argv);
static void cmd_bt_le_test_end(int argc, char **argv);
static void cmd_bt_ext_adv_start(int argc, char **argv);
static void cmd_bt_per_adv_setup(int argc, char **argv);
static void cmd_bt_ead(int argc, char **argv);
static void cmd_bt_selftest(int argc, char **argv);

/* System commands */
static void cmd_help(int argc, char **argv);
static void cmd_rcp_test(int argc, char **argv);
static void cmd_dump(int argc, char **argv);
static void cmd_ps(int argc, char **argv);

/* AT commands */
static void cmd_at_cmd(int argc, char **argv);

typedef struct {
  rcp_data_test_mode_t mode;
  uint32_t duration_ms;
  uint32_t packet_len;
} rcp_test_ctx_t;

static osThreadId_t rcp_test_task_handle;
static rcp_test_ctx_t g_rcp_test_ctx;
static const osThreadAttr_t rcp_test_task_attr = {
    .name = "rcp_test",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityNormal1,
};

/* Command table */
static const shell_cmd_t commands[] = {
    {RCP_MCU_CAP_ANY, "help", cmd_help, "Show this help message"},
	{RCP_MCU_CAP_ANY, "dump", cmd_dump, "Dump host RCP/SDIO/flow-control/buffer status"},
    {RCP_MCU_CAP_ANY, "ps", cmd_ps,
     "Show FreeRTOS task CPU usage"},
    {RCP_MCU_CAP_ANY, "rcp_test", cmd_rcp_test,
     "Run RCP throughput test: rcp_test tx|rx|txrx [duration_s] [packet_len] | "
     "rcp_test stop"},
    {RCP_MCU_CAP_ANY, "atcmd", cmd_at_cmd, "execute an AT command: atcmd AT"},

    	/* HCI commands */
  	{RCP_MCU_CAP_BT, "hci_reset", cmd_hci_reset, "Send HCI Reset command"},
    {RCP_MCU_CAP_BT, "hci_set_event_mask", cmd_hci_set_event_mask, "Send HCI Set Event Mask"},
    {RCP_MCU_CAP_BT, "hci_le_set_event_mask", cmd_hci_le_set_event_mask, "Send LE Set Event Mask"},
    {RCP_MCU_CAP_BT, "hci_read_version", cmd_hci_read_version, "Read Local Version Information"},
    {RCP_MCU_CAP_BT, "bt_adv_start", cmd_bt_adv_start, "Start BLE advertising: bt_adv_start [name]"},
    {RCP_MCU_CAP_BT, "bt_adv_stop", cmd_bt_adv_stop, "Stop BLE advertising"},
    {RCP_MCU_CAP_BT, "bt_connect", cmd_bt_connect, "Connect (Central): bt_connect <addr_type> <XX:XX:XX:XX:XX:XX>"},
    {RCP_MCU_CAP_BT, "bt_add_service", cmd_bt_add_service, "Add example GATT service (call before advertising)"},
	  /* ACI HAL vendor commands (OGF=0x3F) */
    {RCP_MCU_CAP_BT, "bt_set_tx_power", cmd_bt_set_tx_power, "Set TX power: bt_set_tx_power <0-7> (-15..8 dBm)"},
    {RCP_MCU_CAP_BT, "bt_read_rssi", cmd_bt_read_rssi, "Read RSSI of first connected link"},
    {RCP_MCU_CAP_BT, "bt_link_status", cmd_bt_link_status, "Get ACI link status V2"},
    {RCP_MCU_CAP_BT, "bt_radio_activity_mask", cmd_bt_radio_activity_mask, "Set radio activity event mask: bt_radio_activity_mask <hex16>"},
    {RCP_MCU_CAP_BT, "bt_periph_latency", cmd_bt_periph_latency, "Set peripheral latency: bt_periph_latency <0|1>"},
    {RCP_MCU_CAP_BT, "bt_read_config", cmd_bt_read_config, "Read ACI config: bt_read_config <offset> (0=BD,0x35=SC,0xC2=RSSI)"},
    {RCP_MCU_CAP_BT, "bt_set_bd_addr", cmd_bt_set_bd_addr, "Set BD addr: bt_set_bd_addr <b0> <b1> <b2> <b3> <b4> <b5>"},
    {RCP_MCU_CAP_BT, "bt_tone_start", cmd_bt_tone_start, "Start tone test: bt_tone_start <channel> <freq_offset>"},
    {RCP_MCU_CAP_BT, "bt_tone_stop", cmd_bt_tone_stop, "Stop continuous TX / tone test"},
    {RCP_MCU_CAP_BT, "bt_tx_test_pkt_num", cmd_bt_tx_test_pkt_num, "Read TX test packet counter"},
    {RCP_MCU_CAP_BT, "bt_continuous_tx", cmd_bt_continuous_tx, "Start continuous TX: bt_continuous_tx <ch> <phy> <pat>"},
    {RCP_MCU_CAP_BT, "bt_le_tx_test", cmd_bt_le_tx_test, "Start standard LE TX test: bt_le_tx_test <ch> <len> <pat>"},
    {RCP_MCU_CAP_BT, "bt_le_test_end", cmd_bt_le_test_end, "Stop TX/RX test via HCI_LE_TEST_END"},
    {RCP_MCU_CAP_BT, "bt_ext_adv_start", cmd_bt_ext_adv_start, "Start extended advertising (non-connectable): bt_ext_adv_start [hdl=0] [interval_ms=160]"},
    {RCP_MCU_CAP_BT, "bt_per_adv_setup", cmd_bt_per_adv_setup, "Setup+enable periodic adv: bt_per_adv_setup [hdl=0] [interval_ms=40]"},
    {RCP_MCU_CAP_BT, "bt_ead", cmd_bt_ead, "EAD encrypt/decrypt round-trip self-test (no args)"},
    {RCP_MCU_CAP_BT, "bt_selftest", cmd_bt_selftest, "Run ACI HAL self-test (all non-RF commands)"},

    {RCP_MCU_CAP_WIFI, "iperf_c", cmd_iperf_tcp_tx,"Start iperf TCP TX: iperf_tcp_tx <server_ip>"},
    {RCP_MCU_CAP_WIFI, "iperf_s", cmd_iperf_tcp_rx,"Start iperf TCP RX on default port 5001"},
    {RCP_MCU_CAP_WIFI, "iperf_u_c", cmd_iperf_udp_tx,"Start iperf UDP TX: iperf_udp_tx <server_ip>"},
    {RCP_MCU_CAP_WIFI, "iperf_u_s", cmd_iperf_udp_rx,"Start iperf UDP RX on default port 5001"},
    {RCP_MCU_CAP_WIFI, "iperf_stop", cmd_iperf_stop,"Stop the running iperf session"},
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static bool shell_try_command(const shell_cmd_t *cmds, size_t count, int argc,
                              char **argv)
{
  for (size_t i = 0; i < count; i++) {
    if (strcmp(argv[0], cmds[i].name) == 0) {
      if (cmds[i].handler != NULL) {
        cmds[i].handler(argc, argv);
      }
      return true;
    }
  }

  return false;
}

static void shell_print_command_list(const shell_cmd_t *cmds, size_t count)
{
  for (size_t i = 0; i < count; i++) {
    printf("  %-28s - %s\n", cmds[i].name, cmds[i].help);
  }
}

/* Initialize shell */
bool shell_commands_init(void) {
  return true;
}

/* Start shell task */
void shell_task_start(void) {
  if (shell_rx_stream == NULL) {
    shell_rx_stream = xStreamBufferCreateStatic(sizeof(shell_rx_stream_storage),
                                                1, shell_rx_stream_storage,
                                                &shell_rx_stream_struct);
    if (shell_rx_stream == NULL) {
      printf("[ERROR] Failed to create shell RX stream buffer\n");
      return;
    }
  }

  shell_idx = 0;
  shell_rx_dropped = 0;
  shell_last_was_cr = false;
  (void)xStreamBufferReset(shell_rx_stream);
  shell_uart_rx_start();

  shell_task_handle = osThreadNew(shell_task, NULL, &shell_task_attr);
  if (shell_task_handle == NULL) {
    printf("[ERROR] Failed to create shell task\n");
  }
}

static void shell_uart_rx_start(void) {
  (void)HAL_UARTEx_ReceiveToIdle_IT(&huart3, shell_uart_rx_chunk,
                                    sizeof(shell_uart_rx_chunk));
}

static void shell_handle_char(uint8_t ch) {
  if (ch == '\n' && shell_last_was_cr) {
    shell_last_was_cr = false;
    return;
  }

  shell_last_was_cr = (ch == '\r');

	if (ch == '\b' || ch == 127U) {
		if (shell_idx > 0) {
			shell_idx--;
			printf(" \b");

		}
		return;
	}

	if (ch == '\r' || ch == '\n') {
		printf("\n");
		if (shell_idx > 0) {
			shell_buffer[shell_idx] = '\0';
			shell_process_command(shell_buffer);
			shell_idx = 0;
		}
		printf("rcp> ");

		return;
	}

  if (shell_idx < (SHELL_BUF_SIZE - 1U)) {
    shell_buffer[shell_idx++] = (char)ch;
  }
}

/* Shell task implementation */
static void shell_task(void *argument) {
  uint8_t rx_buf[SHELL_UART_RX_CHUNK_SIZE];
  size_t rx_len;
  size_t i;

  (void)argument;

	printf("rcp> ");


  while (1) {
    if (shell_rx_stream == NULL) {
      osDelay(10);
      continue;
    }

    rx_len = xStreamBufferReceive(shell_rx_stream, rx_buf, sizeof(rx_buf),
                                  portMAX_DELAY);
    for (i = 0; i < rx_len; ++i) {
      shell_handle_char(rx_buf[i]);
    }
  }
}

static void rcp_test_task(void *argument) {
  rcp_test_ctx_t ctx = *(rcp_test_ctx_t *)argument;

  (void)start_data_test(ctx.mode, ctx.duration_ms, ctx.packet_len);
  rcp_test_task_handle = NULL;
  osThreadExit();
}

/* Process shell command */
static void shell_process_command(char *cmd) {
  char *argv[10];
  int argc = 0;
  char *token;

  /* Tokenize command */
  token = strtok(cmd, " ");
  while (token != NULL && argc < 10) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }

	if (argc == 0) {
		return;
	}
	if (shell_try_command(commands, NUM_COMMANDS, argc, argv)) {
		return;
	}
	if (shell_try_command(rcp_unit_test_commands, rcp_unit_test_commands_count,
	                      argc, argv)) {
		return;
	}

  printf("Unknown command: %s\n", argv[0]);
  printf("Type 'help' for available commands\n");
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
  BaseType_t higher_priority_task_woken = pdFALSE;
  size_t sent;

  if (huart == NULL || huart->Instance != USART3) {
    return;
  }

  if (size > 0U && shell_rx_stream != NULL) {
    sent = xStreamBufferSendFromISR(shell_rx_stream, shell_uart_rx_chunk, size,
                                    &higher_priority_task_woken);
    if (sent < size) {
      shell_rx_dropped += (uint32_t)(size - sent);
    }
  }

  shell_uart_rx_start();
  portYIELD_FROM_ISR(higher_priority_task_woken);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart == NULL || huart->Instance != USART3) {
    return;
  }

  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                   UART_CLEAR_PEF | UART_CLEAR_FEF |
                                   UART_CLEAR_IDLEF);
  shell_uart_rx_start();
}

/* Help command */
static void cmd_help(int argc, char **argv) {
  printf("\nAvailable commands:\n");
  printf("-------------------\n");
  shell_print_command_list(commands, NUM_COMMANDS);
  printf("\nRCP unit test commands:\n");
  printf("-----------------------\n");
  shell_print_command_list(rcp_unit_test_commands, rcp_unit_test_commands_count);
  printf("\n");
}

static void cmd_dump(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	if (!RCP_HOST_STATS_ENABLED()) {
		printf("dump: disabled (RCP_HOST_STATS_ENABLE=0)\n");
		return;
	}

	printf("\n");
	rcp_stat_dump_all();
	printf("\n");
}

static const char *task_state_name(eTaskState state)
{
  switch (state) {
  case eRunning:
    return "Running";
  case eReady:
    return "Ready";
  case eBlocked:
    return "Blocked";
  case eSuspended:
    return "Suspended";
  case eDeleted:
    return "Deleted";
  default:
    return "Unknown";
  }
}

static const TaskStatus_t *find_task_snapshot(const TaskStatus_t *tasks,
                                              UBaseType_t count,
                                              const TaskStatus_t *task)
{
  for (UBaseType_t i = 0; i < count; i++) {
    if (tasks[i].xHandle == task->xHandle &&
        tasks[i].xTaskNumber == task->xTaskNumber) {
      return &tasks[i];
    }
  }

  return NULL;
}

static void cmd_ps(int argc, char **argv)
{
  UBaseType_t task_count;
  UBaseType_t curr_count;
  TaskStatus_t *curr_tasks;
  configRUN_TIME_COUNTER_TYPE curr_total = 0U;
  configRUN_TIME_COUNTER_TYPE prev_total;
  uint64_t total_delta;
  static TaskStatus_t *ps_prev_tasks;
  static UBaseType_t ps_prev_count;
  static configRUN_TIME_COUNTER_TYPE ps_prev_total;

  (void)argc;
  (void)argv;

  if (!RCP_FREERTOS_CPU_STATS_ENABLED()) {
    printf("ps: disabled (RCP_FREERTOS_CPU_STATS_ENABLE=0)\n");
    return;
  }

  task_count = uxTaskGetNumberOfTasks() + 4U;
  curr_tasks = pvPortMalloc(task_count * sizeof(*curr_tasks));
  if (curr_tasks == NULL) {
    printf("ps: malloc failed\n");
    return;
  }

  curr_count = uxTaskGetSystemState(curr_tasks, task_count, &curr_total);
  if (curr_count == 0U) {
    printf("ps: task snapshot failed\n");
    vPortFree(curr_tasks);
    return;
  }

  prev_total = (ps_prev_tasks == NULL) ? 0U : ps_prev_total;
  total_delta = curr_total - prev_total;
  if (total_delta == 0U) {
    total_delta = 1U;
  }

  printf("\nCPU usage %s, core %lu MHz\n",
         (ps_prev_tasks == NULL) ? "since boot" : "since last ps",
         (unsigned long)(SystemCoreClock / 1000000U));
  printf("%-16s %-9s %-8s %-8s %-10s\n",
         "Task", "State", "Prio", "Stack", "CPU%");
  printf("-------------------------------------------------------\n");

  for (UBaseType_t i = 0; i < curr_count; i++) {
    const TaskStatus_t *prev =
        find_task_snapshot(ps_prev_tasks, ps_prev_count, &curr_tasks[i]);
    uint64_t task_delta;
    unsigned long cpu_x100;

    if (prev == NULL) {
      task_delta = curr_tasks[i].ulRunTimeCounter;
    } else {
      task_delta = curr_tasks[i].ulRunTimeCounter - prev->ulRunTimeCounter;
    }

    cpu_x100 = ((unsigned long long)task_delta * 10000ULL) / total_delta;
    printf("%-16.16s %-9s %2lu/%-4lu %-8lu %3lu.%02lu\n",
           curr_tasks[i].pcTaskName,
           task_state_name(curr_tasks[i].eCurrentState),
           (unsigned long)curr_tasks[i].uxBasePriority,
           (unsigned long)curr_tasks[i].uxCurrentPriority,
           (unsigned long)curr_tasks[i].usStackHighWaterMark,
           cpu_x100 / 100UL,
           cpu_x100 % 100UL);
  }

  vPortFree(ps_prev_tasks);
  ps_prev_tasks = curr_tasks;
  ps_prev_count = curr_count;
  ps_prev_total = curr_total;
}

#define MAX_AT_CMD_LEN 100
static void cmd_at_cmd(int argc, char **argv) {
  char cmd[MAX_AT_CMD_LEN] = {0};

  if (argc == 2) {
    snprintf(cmd, MAX_AT_CMD_LEN, "%s\r\n", argv[1]);
    rcp_tty_send(cmd, strlen(cmd), 3000);
  }
}

static bool iperf_parse_server_ip(const char *ip_str) {
  ip_addr_t server_ip;

  if (ipaddr_aton(ip_str, &server_ip) == 0) {
    printf("Failed to parse IP address: %s\n", ip_str);
    return false;
  }

  return true;
}

static void iperf_warn_extra_args(int argc, int expected_argc) {
  if (argc > expected_argc) {
    printf("Using default port 5001, extra arguments are ignored\n");
  }
}

static void cmd_iperf_tcp_tx(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: iperf_tcp_tx <server_ip>\n");
    return;
  }

  if (!iperf_parse_server_ip(argv[1])) {
    return;
  }

  iperf_warn_extra_args(argc, 2);
  iperf_start(1, 0, argv[1]);
  printf("Started iperf TCP TX to %s on port 5001\n", argv[1]);
}

static void cmd_iperf_tcp_rx(int argc, char **argv) {
  iperf_warn_extra_args(argc, 1);
  iperf_start(0, 0, "0.0.0.0");
  printf("Started iperf TCP RX on port 5001\n");
}

static void cmd_iperf_udp_tx(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: iperf_udp_tx <server_ip>\n");
    return;
  }

  if (!iperf_parse_server_ip(argv[1])) {
    return;
  }

  iperf_warn_extra_args(argc, 2);
  RCP_TX_PROF_RESET();
  iperf_start(1, 1, argv[1]);
  printf("Started iperf UDP TX to %s on port 5001\n", argv[1]);
}

static void cmd_iperf_udp_rx(int argc, char **argv) {
  iperf_warn_extra_args(argc, 1);
  iperf_start(0, 1, "0.0.0.0");
  printf("Started iperf UDP RX on port 5001\n");
}

static void cmd_iperf_stop(int argc, char **argv) {
  TaskHandle_t iperf_handle;

  (void)argc;
  (void)argv;

  iperf_handle = fhost_iperf_msg_handle_get();
  if (iperf_handle == NULL) {
    printf("No running iperf session\n");
    return;
  }

  if (fhost_iperf_sigkill_handler(iperf_handle) != 0) {
    printf("Failed to stop iperf session\n");
  }
}

static void cmd_rcp_test(int argc, char **argv)
{
	rcp_data_test_mode_t mode;
	uint32_t duration_s = 0U;
	uint32_t packet_len = 0U;

  if (argc < 2) {
    printf("usage: rcp_test tx [duration_s] [packet_len]\n");
    printf("       rcp_test rx [duration_s]\n");
    printf("       rcp_test txrx [duration_s] [packet_len]\n");
    printf("       rcp_test stop\n");
    printf("  tx          : host sends raw ETH test packets\n");
    printf("  rx          : host receives raw ETH test packets\n");
    printf("  txrx        : host runs TX and RX together\n");
    printf("  stop        : stop the running throughput test\n");
    printf("  duration_s  : default %lu s for all modes\n",
           (unsigned long)RCP_DATA_TEST_DEFAULT_DURATION_S);
    printf("  packet_len  : default %lu bytes, whole test packet length\n",
           (unsigned long)RCP_DATA_TEST_DEFAULT_PACKET_LEN);
    return;
  }

  if (strcmp(argv[1], "stop") == 0) {
    if (stop_data_test()) {
      printf("RCP data test stop requested\n");
    } else {
      printf("No RCP data test is running\n");
    }
    return;
  }

  if (rcp_test_task_handle != NULL) {
    printf("RCP data test is already running\n");
    return;
  }

  if (strcmp(argv[1], "tx") == 0) {
    mode = RCP_DATA_TEST_MODE_TX;
    duration_s = RCP_DATA_TEST_DEFAULT_DURATION_S;
  } else if (strcmp(argv[1], "rx") == 0) {
    mode = RCP_DATA_TEST_MODE_RX;
    duration_s = RCP_DATA_TEST_DEFAULT_DURATION_S;
  } else if (strcmp(argv[1], "txrx") == 0) {
    mode = RCP_DATA_TEST_MODE_TXRX;
    duration_s = RCP_DATA_TEST_DEFAULT_DURATION_S;
  } else {
    printf("rcp_test mode must be 'tx', 'rx', 'txrx' or 'stop'\n");
    return;
  }

  if (argc >= 3) {
    duration_s = (uint32_t)atoi(argv[2]);
    if (duration_s == 0U) {
      printf("Invalid duration, using default %lu s\n",
             (unsigned long)RCP_DATA_TEST_DEFAULT_DURATION_S);
      duration_s = RCP_DATA_TEST_DEFAULT_DURATION_S;
    }
  }

  if ((mode != RCP_DATA_TEST_MODE_RX) && (argc >= 4)) {
    packet_len = (uint32_t)atoi(argv[3]);
  }

  if ((mode == RCP_DATA_TEST_MODE_RX && argc > 3) ||
      (mode != RCP_DATA_TEST_MODE_RX && argc > 4)) {
    printf("usage: rcp_test tx [duration_s] [packet_len]\n");
    printf("       rcp_test rx [duration_s]\n");
    printf("       rcp_test txrx [duration_s] [packet_len]\n");
    return;
  }

  g_rcp_test_ctx.mode = mode;
  g_rcp_test_ctx.duration_ms = duration_s * 1000U;
  g_rcp_test_ctx.packet_len = packet_len;
  rcp_test_task_handle =
      osThreadNew(rcp_test_task, &g_rcp_test_ctx, &rcp_test_task_attr);
  if (rcp_test_task_handle == NULL) {
    printf("Failed to start RCP data test task\n");
  }
}
/* HCI RX callback - prints received data directly */
static void hci_rx_callback(const uint8_t *data, uint16_t len)
{
    printf("\n[HCI RX] Type: 0x%02X, len=%u\n", data[0], len);
    printf("Data: ");
    for (int i = 0; i < len && i < 32; i++) {
        printf("%02X ", data[i]);
    }
    if (len > 32) {
        printf("...");
    }
	printf("\n");
}

static void hci_cli_init(void)
{
    if (!hci_initialized) {
        printf("Initializing HCI...\n");
        if (bt_hci_init(hci_rx_callback)) {
            printf("✓ HCI initialized successfully\n");
            hci_initialized = true;
        } else {
            printf("✗ HCI initialization failed\n");
        }
    } else {
        printf("HCI already initialized\n");
    }
}

/**
 * @brief CLI command: Send HCI Reset
 * Usage: hci_reset
 */
static void cmd_hci_reset(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n===== HCI Reset Command =====\n");

    hci_cli_init();

    /* HCI Reset: H4_TYPE(0x01) + OCF(0x03) + OGF(0x0C) + ParamLen(0x00) */
    uint8_t hci_reset_cmd[] = {0x01, 0x03, 0x0C, 0x00};

    printf("Sending: 01 03 0C 00\n");

    /* Send command */
    if (!bt_hci_send(hci_reset_cmd, sizeof(hci_reset_cmd))) {
        printf("Failed to send HCI Reset\n");
        return;
    }

}

/**
 * @brief CLI command: Send HCI Set Event Mask
 * Usage: hci_set_event_mask
 */
static void cmd_hci_set_event_mask(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n===== HCI Set Event Mask =====\n");

    hci_cli_init();

    /* Set Event Mask: H4_TYPE + OCF + OGF + ParamLen + Mask[8] */
    uint8_t set_event_mask_cmd[] = {
        0x01,              /* H4 Command */
        0x01, 0x0C,        /* Opcode: 0x0C01 */
        0x08,              /* Parameter length */
        0xFD, 0x8C, 0xE2, 0x02, 0x06, 0xC8, 0x2F, 0x24  /* Event mask */
    };

    printf("Sending: 01 01 0C 08 FD 8C E2 02 06 C8 2F 24\n");

    if (!bt_hci_send(set_event_mask_cmd, sizeof(set_event_mask_cmd))) {
        printf("Failed to send Set Event Mask\n");
        return;
    }

}

/**
 * @brief CLI command: Send LE Set Event Mask
 * Usage: hci_le_set_event_mask
 */
static void cmd_hci_le_set_event_mask(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n===== LE Set Event Mask =====\n");

    hci_cli_init();

    /* LE Set Event Mask: H4_TYPE + OCF + OGF + ParamLen + Mask[8] */
    uint8_t le_set_event_mask_cmd[] = {
        0x01,              /* H4 Command */
        0x01, 0x20,        /* Opcode: 0x2001 */
        0x08,              /* Parameter length */
        0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x00  /* LE event mask */
    };

    printf("Sending: 01 01 20 08 FF FF FF FF 03 00 00 00\n");

    if (!bt_hci_send(le_set_event_mask_cmd, sizeof(le_set_event_mask_cmd))) {
        printf("Failed to send LE Set Event Mask\n");
        return;
    }

}

/**
 * @brief CLI command: Read Local Version
 * Usage: hci_read_version
 */
static void cmd_hci_read_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n===== Read Local Version =====\n");

    hci_cli_init();

    /* Read Local Version Info: H4_TYPE + OCF + OGF + ParamLen */
    uint8_t read_version_cmd[] = {0x01, 0x01, 0x10, 0x00};

    printf("Sending: 01 01 10 00\n");

    if (!bt_hci_send(read_version_cmd, sizeof(read_version_cmd))) {
        printf("Failed to send Read Local Version\n");
        return;
    }

}

/**
 * @brief CLI command: Send raw HCI data
 * Usage: hci_send <hex_bytes>
 * Example: hci_send 01 03 0C 00
 */
static void cmd_hci_send(int argc, char **argv)
{
    uint8_t hci_data[256];

    if (argc < 2) {
        printf("Usage: hci_send <hex_bytes>\n");
        printf("Examples:\n");
        printf("  hci_send 01 03 0C 00           (HCI Reset)\n");
        printf("  hci_send 01 01 0C 08 FD 8C...  (Set Event Mask)\n");
        printf("  hci_send 02 00 01 00 0C        (ACL Data)\n");
        printf("H4 types: 01=CMD, 02=ACL, 03=SCO, 04=EVT\n");
        return;
    }

    printf("\n===== Send Raw HCI =====\n");

    hci_cli_init();

    /* Parse hex bytes */
    uint8_t total_len = 0;
    for (int i = 1; i < argc && total_len < sizeof(hci_data); i++) {
        unsigned int byte_val;
        if (sscanf(argv[i], "%02x", &byte_val) == 1 || sscanf(argv[i], "%02X", &byte_val) == 1) {
            hci_data[total_len++] = (uint8_t)byte_val;
        } else {
            printf("Invalid hex value: %s\n", argv[i]);
            return;
        }
    }

    if (total_len < 1) {
        printf("Error: No data provided\n");
        return;
    }

    /* Show H4 packet type */
    const char *type_str = "Unknown";
    switch (hci_data[0]) {
        case 0x01: type_str = "HCI Command"; break;
        case 0x02: type_str = "ACL Data"; break;
        case 0x03: type_str = "SCO Data"; break;
        case 0x04: type_str = "HCI Event"; break;
    }
    printf("Sending %s (%u bytes)\n", type_str, total_len);

    /* Send data */
    printf("TX: ");
    for (int i = 0; i < total_len && i < 32; i++) {
        printf("%02X ", hci_data[i]);
    }
    if (total_len > 32) {
        printf("...");
    }
    printf("\n");

    if (!bt_hci_send(hci_data, total_len)) {
        printf("Failed to send HCI data\n");
        return;
    }

}

static uint8_t parse_mac(const char *str, uint8_t *out)
{
    unsigned int b[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &b[0],&b[1],&b[2],&b[3],&b[4],&b[5]) != 6)
        return 0;
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)b[i];
    return 1;
}

static void cmd_bt_connect(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: bt_connect <addr_type> <XX:XX:XX:XX:XX:XX>\n");
        printf("  addr_type: 0=public 1=random\n");
        return;
    }
    uint8_t addr_type = (uint8_t)atoi(argv[1]);
    uint8_t addr[6] = {0};
    if (!parse_mac(argv[2], addr)) {
        printf("Invalid MAC address format\n");
        return;
    }
    Init_Param_Phy_t phy_params = {
        .Scan_Interval       = 0x0060,  /* 60ms */
        .Scan_Window         = 0x0030,  /* 30ms */
        .Conn_Interval_Min   = 0x0006,  /* 7.5ms */
        .Conn_Interval_Max   = 0x0020,  /* 40ms */
        .Conn_Latency        = 0x0000,
        .Supervision_Timeout = 0x002A,  /* 420ms */
        .Min_CE_Length       = 0x0000,
        .Max_CE_Length       = 0xFFFF,
    };
    /* Reverse MAC: user inputs LSB-first (b0:6a:...), HCI needs MSB-first (7c:bc:...) */
    uint8_t addr_rev[6];
    for (int i = 0; i < 6; i++) addr_rev[i] = addr[5 - i];

    tBleStatus ret = hci_le_extended_create_connection(
        0x00 /* no filter policy */,
        0x00 /* own addr: public */,
        addr_type, addr_rev,
        0x01 /* Initiating PHYs: LE 1M */,
        &phy_params);
    printf("hci_le_extended_create_connection(type=%d %02X:%02X:%02X:%02X:%02X:%02X) = 0x%02X\n",
           addr_type, addr_rev[0],addr_rev[1],addr_rev[2],addr_rev[3],addr_rev[4],addr_rev[5], ret);
}

static void cmd_bt_adv_start(int argc, char **argv)
{
    uint8_t dev_name[32];
    uint8_t dev_name_len;

    if (argc >= 2) {
        /* bt_adv_start <name> */
        dev_name[0] = 0x09; /* AD_TYPE_COMPLETE_LOCAL_NAME */
        size_t name_len = strlen(argv[1]);
        if (name_len > sizeof(dev_name) - 1)
            name_len = sizeof(dev_name) - 1;
        memcpy(dev_name + 1, argv[1], name_len);
        dev_name_len = (uint8_t)(1 + name_len);
    } else {
        /* default name */
        const uint8_t default_name[] = {0x09, 'Q','C','C','7','4','x','-','R','C','P'};
        memcpy(dev_name, default_name, sizeof(default_name));
        dev_name_len = sizeof(default_name);
    }

    if (BleAppAdvStart(0x00 /*ADV_IND*/, 0x0020, 0x0020, dev_name, dev_name_len)) {
        printf("BLE advertising started\n");
    } else {
        printf("Failed to start BLE advertising\n");
    }
}

static void cmd_bt_adv_stop(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (BleAppAdvStop()) {
        printf("BLE advertising stopped\n");
    } else {
        printf("Failed to stop BLE advertising\n");
    }
}

static void cmd_bt_add_service(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (BleAppAddService()) {
        printf("GATT service added\n");
    } else {
        printf("Failed to add GATT service\n");
    }
}

/* ============================================================================
   ACI HAL Vendor Commands (OGF=0x3F) — via ble_hal_aci.h API
   ============================================================================
 */

static void cmd_bt_set_tx_power(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: bt_set_tx_power <pa_level 0-7>\n");
        printf("  0=-15 1=-9 2=-6 3=-2 4=0 5=2 6=5 7=8 dBm\n");
        return;
    }
    int level = atoi(argv[1]);
    if (level < 0 || level > 7) {
        printf("pa_level must be 0-7\n");
        return;
    }
    tBleStatus ret = aci_hal_set_tx_power_level(0, (uint8_t)level);
    printf("aci_hal_set_tx_power_level(%d) = 0x%02X\n", level, ret);
}

static void cmd_bt_read_rssi(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint8_t rssi = 127;
    tBleStatus ret = aci_hal_read_rssi(&rssi);
    printf("aci_hal_read_rssi = 0x%02X, rssi=%d dBm\n", ret, (int8_t)rssi);
}

static void cmd_bt_link_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint8_t link_status[22];
    uint16_t conn_handle[22];
    tBleStatus ret = aci_hal_get_link_status_v2(link_status, conn_handle);
    printf("aci_hal_get_link_status_v2 = 0x%02X\n", ret);
    if (ret == BLE_STATUS_SUCCESS) {
        for (int i = 0; i < 22; i++) {
            if (link_status[i] == 0x00) break;
            printf("  [%d] status=0x%02X handle=0x%04X\n", i, link_status[i], conn_handle[i]);
        }
    }
}

static void cmd_bt_radio_activity_mask(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: bt_radio_activity_mask <hex16>\n");
        printf("  e.g. ffff (all), 0002 (ADV only)\n");
        return;
    }
    uint16_t mask = (uint16_t)strtoul(argv[1], NULL, 16);
    tBleStatus ret = aci_hal_set_radio_activity_mask(mask);
    printf("aci_hal_set_radio_activity_mask(0x%04X) = 0x%02X\n", mask, ret);
}

static void cmd_bt_periph_latency(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: bt_periph_latency <0|1>\n");
        return;
    }
    uint8_t enable = (uint8_t)atoi(argv[1]);
    tBleStatus ret = aci_hal_set_peripheral_latency(enable);
    printf("aci_hal_set_peripheral_latency(%d) = 0x%02X\n", enable, ret);
}

static void cmd_bt_read_config(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: bt_read_config <offset>\n");
        printf("  0x00=BD addr  0x35=SC key type  0xC2=RSSI golden range\n");
        return;
    }
    uint8_t offset = (uint8_t)strtoul(argv[1], NULL, 0);
    uint8_t data[32];
    uint8_t data_len = 0;
    tBleStatus ret = aci_hal_read_config_data(offset, &data_len, data);
    printf("aci_hal_read_config_data(0x%02X) = 0x%02X, len=%d, data=", offset, ret, data_len);
    for (int i = 0; i < data_len; i++) printf("%02X ", data[i]);
    printf("\n");
}

static void cmd_bt_set_bd_addr(int argc, char **argv)
{
    if (argc < 7) {
        printf("Usage: bt_set_bd_addr <b0> <b1> <b2> <b3> <b4> <b5>\n");
        printf("  e.g. bt_set_bd_addr 11 22 33 44 55 66\n");
        return;
    }
    uint8_t addr[6];
    for (int i = 0; i < 6; i++) {
        addr[i] = (uint8_t)strtoul(argv[i + 1], NULL, 16);
    }
    tBleStatus ret = aci_hal_write_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, 6, addr);
    printf("aci_hal_write_config_data(BD_ADDR) = 0x%02X\n", ret);
    if (ret == BLE_STATUS_SUCCESS) {
        uint8_t data[6];
        uint8_t data_len = 0;
        aci_hal_read_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, &data_len, data);
        printf("  read back: %02X:%02X:%02X:%02X:%02X:%02X\n",
               data[0], data[1], data[2], data[3], data[4], data[5]);
    }
}

static void cmd_bt_tone_start(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: bt_tone_start <channel 0x00-0x27> <freq_offset 0x00-0xFF>\n");
        return;
    }
    uint8_t channel = (uint8_t)strtol(argv[1], NULL, 0);
    uint8_t freq_offset = (uint8_t)strtol(argv[2], NULL, 0);
    tBleStatus ret = aci_hal_tone_start(channel, freq_offset);
    printf("aci_hal_tone_start = 0x%02X (channel=%u, freq_offset=%u)\n", ret, channel, freq_offset);
}

static void cmd_bt_tone_stop(int argc, char **argv)
{
    (void)argc; (void)argv;
    tBleStatus ret = aci_hal_tone_stop();
    printf("aci_hal_tone_stop = 0x%02X\n", ret);
}

static void cmd_bt_tx_test_pkt_num(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t nb_pkt = 0;
    tBleStatus ret = aci_hal_le_tx_test_packet_number(&nb_pkt);
    printf("aci_hal_le_tx_test_packet_number = 0x%02X, nb_pkt=%lu\n", ret, (unsigned long)nb_pkt);
}

static void cmd_bt_continuous_tx(int argc, char **argv)
{
    if (argc < 4) {
        printf("Usage: bt_continuous_tx <channel> <phy> <pattern>\n");
        printf("  channel: 0-39\n");
        printf("  phy: 1=1M 2=2M\n");
        printf("  pattern: 0=PRBS9 1=0xFF11110000 2=0xAA10101010 3=PRBS15 4=All1 5=All0\n");
        return;
    }
    uint8_t channel = (uint8_t)atoi(argv[1]);
    uint8_t phy     = (uint8_t)atoi(argv[2]);
    uint8_t pattern = (uint8_t)atoi(argv[3]);
    tBleStatus ret = aci_hal_continuous_tx_start(channel, phy, pattern);
    printf("aci_hal_continuous_tx_start(ch=%d phy=%d pat=%d) = 0x%02X\n", channel, phy, pattern, ret);
}

static void cmd_bt_le_tx_test(int argc, char **argv)
{
    if (argc < 4) {
        printf("Usage: bt_le_tx_test <channel> <len> <pattern>\n");
        printf("  channel: 0-39\n");
        printf("  len: 0-255 bytes\n");
        printf("  pattern: 0=PRBS9 1=0xF0 2=0xAA 3=PRBS15 4=All1 5=All0\n");
        return;
    }
    uint8_t channel = (uint8_t)atoi(argv[1]);
    uint8_t len     = (uint8_t)atoi(argv[2]);
    uint8_t pattern = (uint8_t)atoi(argv[3]);
    tBleStatus ret = hci_le_transmitter_test(channel, len, pattern);
    printf("hci_le_transmitter_test(ch=%d len=%d pat=%d) = 0x%02X\n", channel, len, pattern, ret);
}

static void cmd_bt_le_test_end(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint16_t nb_pkt = 0xFFFF;
    tBleStatus ret = hci_le_test_end(&nb_pkt);
    printf("hci_le_test_end = 0x%02X, nb_pkt=%u\n", ret, nb_pkt);
}

static void cmd_bt_ext_adv_start(int argc, char **argv)
{
    uint8_t  adv_hdl     = (argc > 1) ? (uint8_t)atoi(argv[1]) : 0;
    uint32_t interval_ms = (argc > 2) ? (uint32_t)atoi(argv[2]) : 160;
    /* interval in 0.625us units, encoded as 3-byte array */
    uint32_t interval = (interval_ms * 1000) / 625;
    uint8_t interval_min[3] = { interval & 0xFF, (interval >> 8) & 0xFF, (interval >> 16) & 0xFF };
    uint8_t interval_max[3] = { interval_min[0], interval_min[1], interval_min[2] };
    uint8_t peer_addr[6] = {0};
    tBleStatus ret;
    int8_t sel_tx_power;

    /* Non-connectable, non-scannable, non-legacy extended advertising (required for periodic adv + BIG) */
    ret = hci_le_set_extended_advertising_parameters(adv_hdl,
                                                     0x0000 /* Properties: ext non-conn non-scan */,
                                                     interval_min, interval_max,
                                                     0x07 /* channel map: all */,
                                                     0x00 /* Own addr: public */,
                                                     0x00 /* Peer addr type: public */,
                                                     peer_addr,
                                                     0x00 /* Filter policy */,
                                                     0x09 /* TX power: +6 dBm default */,
                                                     0x01 /* Primary PHY: 1M */,
                                                     0x00 /* Max skip */,
                                                     0x01 /* Secondary PHY: 1M */,
                                                     0x00 /* SID */,
                                                     0x00 /* Scan req notification: disable */,
                                                     &sel_tx_power);
    printf("hci_le_set_extended_advertising_parameters(hdl=%d interval=%lums tx_pwr=%d) = 0x%02X\n",
           adv_hdl, (unsigned long)interval_ms, (int)sel_tx_power, ret);
    if (ret != BLE_STATUS_SUCCESS) return;

    /* Set minimal advertising data (complete local name) */
    const uint8_t adv_data[] = {0x0B, 0x09, 'P','E','R','I','O','D','-','B','I','G'};
    ret = hci_le_set_extended_advertising_data(adv_hdl, 0x03 /* complete */, 0x00 /* may fragment */,
                                               sizeof(adv_data), adv_data);
    printf("hci_le_set_extended_advertising_data = 0x%02X\n", ret);
    if (ret != BLE_STATUS_SUCCESS) return;

    /* Enable advertising set */
    Adv_Set_t adv_set = { .Advertising_Handle = adv_hdl,
                          .Duration = 0 /* no duration limit */,
                          .Max_Extended_Advertising_Events = 0 };
    ret = hci_le_set_extended_advertising_enable(0x01 /* enable */, 1, &adv_set);
    printf("hci_le_set_extended_advertising_enable(hdl=%d) = 0x%02X\n", adv_hdl, ret);
}

static void cmd_bt_per_adv_setup(int argc, char **argv)
{
    uint8_t  adv_hdl     = (argc > 1) ? (uint8_t)atoi(argv[1]) : 0;
    uint32_t interval_ms = (argc > 2) ? (uint32_t)atoi(argv[2]) : 40;
    /* periodic interval in 1.25ms units */
    uint16_t per_interval = (uint16_t)((interval_ms * 1000) / 1250);
    tBleStatus ret;

    /* Step 1: Set periodic advertising parameters */
    ret = hci_le_set_periodic_advertising_parameters(adv_hdl,
                                                     per_interval, per_interval,
                                                     0x0000 /* Properties */);
    printf("hci_le_set_periodic_advertising_parameters(hdl=%d interval=%lums) = 0x%02X\n",
           adv_hdl, (unsigned long)interval_ms, ret);
    if (ret != BLE_STATUS_SUCCESS) return;

    /* Step 2: Set periodic advertising data */
    const uint8_t per_data[] = {0x02, 0x01, 0x06 /* Flags + LE General Discoverable */};
    ret = hci_le_set_periodic_advertising_data(adv_hdl, 0x03 /* complete */, sizeof(per_data), per_data);
    printf("hci_le_set_periodic_advertising_data(len=%d) = 0x%02X\n", (int)sizeof(per_data), ret);
    if (ret != BLE_STATUS_SUCCESS) return;

    /* Step 3: Enable periodic advertising (Enable=0x01, no ADI bit) */
    ret = hci_le_set_periodic_advertising_enable(0x01 /* enable */, adv_hdl);
    printf("hci_le_set_periodic_advertising_enable(hdl=%d) = 0x%02X\n", adv_hdl, ret);
    printf("Periodic advertising enabled (hdl=%d)\n", adv_hdl);
}

static void cmd_bt_ead(int argc, char **argv)
{
    (void)argc; (void)argv;

    const uint8_t key[16]   = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                               0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    const uint8_t iv[8]     = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
    const uint8_t plain[13] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x57,
                               0x6F, 0x72, 0x6C, 0x64, 0x21};
    uint8_t enc[13 + 9];
    uint8_t dec[13];
    uint16_t enc_len = 0, dec_len = 0;

    tBleStatus ret = aci_hal_ead_encrypt_decrypt(0, key, iv,
                                                  sizeof(plain), plain,
                                                  &enc_len, enc);
    if (ret != BLE_STATUS_SUCCESS) {
        printf("EAD encrypt FAILED: 0x%02X\n", ret);
        printf("TEST: FAIL\n");
        return;
    }
    printf("EAD encrypt OK, ciphertext (%d bytes): ", enc_len);
    for (int i = 0; i < enc_len; i++) printf("%02X ", enc[i]);
    printf("\n");

    ret = aci_hal_ead_encrypt_decrypt(1, key, iv, enc_len, enc, &dec_len, dec);
    if (ret != BLE_STATUS_SUCCESS) {
        printf("EAD decrypt FAILED: 0x%02X\n", ret);
        printf("TEST: FAIL\n");
        return;
    }
    printf("EAD decrypt OK, plaintext (%d bytes): ", dec_len);
    for (int i = 0; i < dec_len; i++) printf("%02X ", dec[i]);
    printf("\n");

    if (dec_len == sizeof(plain) && memcmp(dec, plain, sizeof(plain)) == 0) {
        printf("TEST: PASS\n");
    } else {
        printf("TEST: FAIL (round-trip mismatch)\n");
    }
}

/* ============================================================================
   bt_test — one-shot ACI HAL self-test
   Tests: SET_TX_POWER, WRITE/READ_CONFIG_DATA, EAD encrypt/decrypt round-trip,
          GET_LINK_STATUS_V2, SET_RADIO_ACTIVITY_MASK, SET_PERIPHERAL_LATENCY,
          SC_KEY_TYPE debug override, abnormal-parameter coverage.
   Note: Test 9 and 10g/10h were removed together with SET_EVENT_MASK.
   ============================================================================ */

static int bt_test_pass;
static int bt_test_fail;

#define BT_TEST_ASSERT(name, expr) do { \
    if (expr) { \
        printf("  [PASS] %s\n", name); \
        bt_test_pass++; \
    } else { \
        printf("  [FAIL] %s\n", name); \
        bt_test_fail++; \
    } \
} while(0)

static void cmd_bt_selftest(int argc, char **argv)
{
    (void)argc; (void)argv;
    tBleStatus ret;

    bt_test_pass = 0;
    bt_test_fail = 0;

    printf("\n===== ACI HAL Self-Test =====\n\n");

    /* --- Test 1: SET_TX_POWER_LEVEL --- */
    printf("--- SET_TX_POWER_LEVEL ---\n");
    ret = aci_hal_set_tx_power_level(0, 4);
    BT_TEST_ASSERT("set_tx_power_level(4) returns SUCCESS", ret == BLE_STATUS_SUCCESS);
    ret = aci_hal_set_tx_power_level(0, 0);
    BT_TEST_ASSERT("set_tx_power_level(0) returns SUCCESS", ret == BLE_STATUS_SUCCESS);

    /* --- Test 2: WRITE_CONFIG_DATA + READ_CONFIG_DATA (write->read verify) --- */
    printf("\n--- WRITE/READ_CONFIG_DATA ---\n");
    {
        uint8_t data[32];
        uint8_t data_len = 0;

        /* Save original values so we can restore each touched field at end */
        uint8_t orig_bd[6];
        uint8_t orig_sc_key_type[1];
        uint8_t orig_rssi_range[2];
        ret = aci_hal_read_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, &data_len, orig_bd);
        BT_TEST_ASSERT("read_config_data(BD_ADDR) returns SUCCESS", ret == BLE_STATUS_SUCCESS);
        aci_hal_read_config_data(CONFIG_DATA_SC_KEY_TYPE_OFFSET, &data_len, orig_sc_key_type);
        aci_hal_read_config_data(CONFIG_DATA_LL_RSSI_GOLDEN_RANGE_OFFSET, &data_len, orig_rssi_range);

        /* SC_KEY_TYPE */
        ret = aci_hal_write_config_data(CONFIG_DATA_SC_KEY_TYPE_OFFSET, 1, (const uint8_t[]){0xAB});
        BT_TEST_ASSERT("write_config_data(SC_KEY_TYPE=0xAB) SUCCESS", ret == BLE_STATUS_SUCCESS);
        ret = aci_hal_read_config_data(CONFIG_DATA_SC_KEY_TYPE_OFFSET, &data_len, data);
        BT_TEST_ASSERT("read_config_data(SC_KEY_TYPE) returns SUCCESS", ret == BLE_STATUS_SUCCESS);
        BT_TEST_ASSERT("read back SC_KEY_TYPE == 0xAB", data[0] == 0xAB);

        /* RSSI golden range */
        uint8_t rssi_test[2] = {0x80, 0xC0};
        ret = aci_hal_write_config_data(CONFIG_DATA_LL_RSSI_GOLDEN_RANGE_OFFSET, 2, rssi_test);
        BT_TEST_ASSERT("write_config_data(RSSI_RANGE) SUCCESS", ret == BLE_STATUS_SUCCESS);
        ret = aci_hal_read_config_data(CONFIG_DATA_LL_RSSI_GOLDEN_RANGE_OFFSET, &data_len, data);
        BT_TEST_ASSERT("read_config_data(RSSI_RANGE) returns SUCCESS", ret == BLE_STATUS_SUCCESS);
        BT_TEST_ASSERT("read back RSSI_RANGE matches",
                       data[0] == rssi_test[0] && data[1] == rssi_test[1]);

        /* BD addr */
        uint8_t test_bd[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        ret = aci_hal_write_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, 6, test_bd);
        BT_TEST_ASSERT("write_config_data(BD_ADDR) SUCCESS", ret == BLE_STATUS_SUCCESS);
        ret = aci_hal_read_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, &data_len, data);
        BT_TEST_ASSERT("read_config_data(BD_ADDR) returns SUCCESS", ret == BLE_STATUS_SUCCESS);
        BT_TEST_ASSERT("read back BD_ADDR matches",
                       data[0] == 0x11 && data[1] == 0x22 && data[2] == 0x33 &&
                       data[3] == 0x44 && data[4] == 0x55 && data[5] == 0x66);

        /* Restore all touched fields */
        aci_hal_write_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, 6, orig_bd);
        aci_hal_write_config_data(CONFIG_DATA_SC_KEY_TYPE_OFFSET, 1, orig_sc_key_type);
        aci_hal_write_config_data(CONFIG_DATA_LL_RSSI_GOLDEN_RANGE_OFFSET, 2, orig_rssi_range);
        printf("  BD addr: %02X:%02X:%02X:%02X:%02X:%02X (restored)\n",
               orig_bd[5], orig_bd[4], orig_bd[3], orig_bd[2], orig_bd[1], orig_bd[0]);
        printf("  SC_KEY_TYPE, RSSI_GOLDEN_RANGE: restored to original\n");
    }

    /* --- Test 3: EAD encrypt + decrypt round-trip --- */
    printf("\n--- EAD ENCRYPT/DECRYPT ROUND-TRIP ---\n");
    {
        const uint8_t key[16]  = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                  0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
        const uint8_t iv[8]    = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
        const uint8_t plain[]  = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x57,
                                  0x6F, 0x72, 0x6C, 0x64, 0x21};
        uint8_t enc[64], dec[64];
        uint16_t enc_len = 0, dec_len = 0;

        ret = aci_hal_ead_encrypt_decrypt(0, key, iv, sizeof(plain), plain, &enc_len, enc);
        BT_TEST_ASSERT("ead_encrypt returns SUCCESS", ret == BLE_STATUS_SUCCESS);
        BT_TEST_ASSERT("ead_encrypt out_len == in_len + 9", enc_len == sizeof(plain) + 9);

        ret = aci_hal_ead_encrypt_decrypt(1, key, iv, enc_len, enc, &dec_len, dec);
        BT_TEST_ASSERT("ead_decrypt returns SUCCESS", ret == BLE_STATUS_SUCCESS);
        BT_TEST_ASSERT("ead_decrypt out_len == original plaintext len", dec_len == sizeof(plain));
        BT_TEST_ASSERT("ead round-trip data matches",
                       dec_len == (uint16_t)sizeof(plain) && memcmp(dec, plain, sizeof(plain)) == 0);

        printf("  plaintext:  ");
        for (int i = 0; i < (int)sizeof(plain); i++) printf("%02X ", plain[i]);
        printf("\n  encrypted: ");
        for (int i = 0; i < (int)enc_len; i++) printf("%02X ", enc[i]);
        printf("\n  decrypted: ");
        for (int i = 0; i < (int)dec_len; i++) printf("%02X ", dec[i]);
        printf("\n");
    }

    /* --- Test 4: EAD decrypt with wrong key --- */
    printf("\n--- EAD DECRYPT WITH WRONG KEY ---\n");
    {
        const uint8_t key[16]        = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
        const uint8_t wrong_key[16]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        const uint8_t iv[8]          = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        const uint8_t plain[]        = {0xDE, 0xAD, 0xBE, 0xEF};
        uint8_t enc[64], dec[64];
        uint16_t enc_len = 0, dec_len = 0;

        ret = aci_hal_ead_encrypt_decrypt(0, key, iv, sizeof(plain), plain, &enc_len, enc);
        BT_TEST_ASSERT("ead_encrypt (wrong-key prep) SUCCESS", ret == BLE_STATUS_SUCCESS);

        ret = aci_hal_ead_encrypt_decrypt(1, wrong_key, iv, enc_len, enc, &dec_len, dec);
        BT_TEST_ASSERT("ead_decrypt with wrong key returns FAILED", ret != BLE_STATUS_SUCCESS);
    }

    /* --- Test 5: GET_LINK_STATUS_V2 --- */
    printf("\n--- GET_LINK_STATUS_V2 ---\n");
    {
        uint8_t link_status[22];
        uint16_t conn_handle[22];
        ret = aci_hal_get_link_status_v2(link_status, conn_handle);
        BT_TEST_ASSERT("get_link_status_v2 returns SUCCESS", ret == BLE_STATUS_SUCCESS);

        bool found_conn = false;
        for (int i = 0; i < 22 && !found_conn; i++) {
            if (link_status[i] == 0x00) break;
            printf("  [%d] status=0x%02X handle=0x%04X\n", i, link_status[i], conn_handle[i]);
            if (link_status[i] == 0x02 || link_status[i] == 0x05) found_conn = true;
        }
        if (!found_conn) {
            printf("  (no active connections)\n");
        }

        if (found_conn) {
            uint8_t rssi = 127;
            ret = aci_hal_read_rssi(&rssi);
            BT_TEST_ASSERT("read_rssi returns SUCCESS", ret == BLE_STATUS_SUCCESS);
            printf("  rssi = %d dBm\n", (int8_t)rssi);
        } else {
            printf("  read_rssi: skipped (no connection)\n");
        }
    }

    /* --- Test 6: SET_RADIO_ACTIVITY_MASK --- */
    printf("\n--- SET_RADIO_ACTIVITY_MASK ---\n");
    ret = aci_hal_set_radio_activity_mask(0xFFFF);
    BT_TEST_ASSERT("set_radio_activity_mask(0xFFFF) SUCCESS", ret == BLE_STATUS_SUCCESS);
    ret = aci_hal_set_radio_activity_mask(0x0000);
    BT_TEST_ASSERT("set_radio_activity_mask(0x0000) SUCCESS", ret == BLE_STATUS_SUCCESS);

    /* --- Test 7: SET_PERIPHERAL_LATENCY --- */
    printf("\n--- SET_PERIPHERAL_LATENCY ---\n");
    ret = aci_hal_set_peripheral_latency(0);
    BT_TEST_ASSERT("set_peripheral_latency(0=disable) SUCCESS", ret == BLE_STATUS_SUCCESS);
    ret = aci_hal_set_peripheral_latency(1);
    BT_TEST_ASSERT("set_peripheral_latency(1=enable) SUCCESS", ret == BLE_STATUS_SUCCESS);
    ret = aci_hal_set_peripheral_latency(0);
    BT_TEST_ASSERT("set_peripheral_latency(0=restore) SUCCESS", ret == BLE_STATUS_SUCCESS);

    /* --- Test 8: SC_KEY_TYPE debug override --- */
    printf("\n--- SC_KEY_TYPE DEBUG OVERRIDE ---\n");
    {
        /* Force SC_KEY_TYPE=GEN so the controller takes the "needs generated
         * priv key OR debug override" branch (defensive — earlier tests may
         * have left a different value). */
        aci_hal_write_config_data(CONFIG_DATA_SC_KEY_TYPE_OFFSET, 1, (const uint8_t[]){0x00});

        /* key_type=GEN with an invalid (all-zero) peer pubkey must not succeed:
         * either rejected before ECC (no priv key) or ECC fails (invalid point). */
        const uint8_t zero_pub_key[64] = {0};
        ret = hci_le_generate_dhkey_v2(zero_pub_key, 0x00);
        tBleStatus first_ret = ret;
        BT_TEST_ASSERT("gen_dhkey_v2(key_type=GEN, zero pubkey) => not SUCCESS",
                       first_ret != BLE_STATUS_SUCCESS);

        /* SC_KEY_TYPE=DEBUG forces debug key. Use matching debug public key so
         * ECC DH computation succeeds, proving the override took effect. */
        const uint8_t debug_pub_key[64] = {
            /* DebugE256PublicKey_x */
            0xE6, 0x9D, 0x35, 0x0E, 0x48, 0x01, 0x03, 0xCC, 0xDB, 0xFD, 0xF4, 0xAC, 0x11, 0x91, 0xF4, 0xEF,
            0xB9, 0xA5, 0xF9, 0xE9, 0xA7, 0x83, 0x2C, 0x5E, 0x2C, 0xBE, 0x97, 0xF2, 0xD2, 0x03, 0xB0, 0x20,
            /* DebugE256PublicKey_y */
            0x8B, 0xD2, 0x89, 0x15, 0xD0, 0x8E, 0x1C, 0x74, 0x24, 0x30, 0xED, 0x8F, 0xC2, 0x45, 0x63, 0x76,
            0x5C, 0x15, 0x52, 0x5A, 0xBF, 0x9A, 0x32, 0x63, 0x6D, 0xEB, 0x2A, 0x65, 0x49, 0x9C, 0x80, 0xDC,
        };
        aci_hal_write_config_data(CONFIG_DATA_SC_KEY_TYPE_OFFSET, 1, (const uint8_t[]){0x01});
        ret = hci_le_generate_dhkey_v2(debug_pub_key, 0x00);
        BT_TEST_ASSERT("gen_dhkey_v2(key_type=GEN, SC_KEY_TYPE=DEBUG, valid pubkey) => SUCCESS",
                       ret == BLE_STATUS_SUCCESS);

        /* Restore SC_KEY_TYPE to normal */
        aci_hal_write_config_data(CONFIG_DATA_SC_KEY_TYPE_OFFSET, 1, (const uint8_t[]){0x00});
    }

    /* --- Test 10: Abnormal parameters --- */
    printf("\n--- ABNORMAL PARAMETER TESTS ---\n");

    /* 10a: SET_TX_POWER_LEVEL — PA_Level valid: 0x00..0x23 */
    ret = aci_hal_set_tx_power_level(0, 0x24);
    BT_TEST_ASSERT("set_tx_power_level(PA=0x24) out of range => error",
                   ret != BLE_STATUS_SUCCESS);
    ret = aci_hal_set_tx_power_level(0, 0xFF);
    BT_TEST_ASSERT("set_tx_power_level(PA=0xFF) out of range => error",
                   ret != BLE_STATUS_SUCCESS);
    ret = aci_hal_set_tx_power_level(0x02, 0x10);
    BT_TEST_ASSERT("set_tx_power_level(En=0x02) deprecated/ignored => SUCCESS",
                   ret == BLE_STATUS_SUCCESS);

    /* 10b: WRITE_CONFIG_DATA abnormal parameters */
    {
        /* Invalid offsets */
        ret = aci_hal_write_config_data(0x01, 1, (const uint8_t[]){0x00});
        BT_TEST_ASSERT("write_config_data(offset=0x01) invalid offset => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_write_config_data(0x50, 1, (const uint8_t[]){0x00});
        BT_TEST_ASSERT("write_config_data(offset=0x50) invalid offset => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_write_config_data(0xFE, 1, (const uint8_t[]){0x00});
        BT_TEST_ASSERT("write_config_data(offset=0xFE) invalid offset => error",
                       ret != BLE_STATUS_SUCCESS);

        /* Valid offset, wrong length */
        ret = aci_hal_write_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, 1, (const uint8_t[]){0x00});
        BT_TEST_ASSERT("write_config_data(BD_ADDR, len=1) wrong len => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_write_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, 0xFF, (const uint8_t[]){0x00});
        BT_TEST_ASSERT("write_config_data(BD_ADDR, len=0xFF) wrong len => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_write_config_data(CONFIG_DATA_ER_OFFSET, 1, (const uint8_t[]){0x00});
        BT_TEST_ASSERT("write_config_data(ER_KEY, len=1) wrong len => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_write_config_data(CONFIG_DATA_SC_KEY_TYPE_OFFSET, 6, (const uint8_t[]){0, 0, 0, 0, 0, 0});
        BT_TEST_ASSERT("write_config_data(SC_KEY_TYPE, len=6) wrong len => error",
                       ret != BLE_STATUS_SUCCESS);

        /* Zero length */
        ret = aci_hal_write_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, 0, (const uint8_t[]){0x00});
        BT_TEST_ASSERT("write_config_data(len=0) zero length => error",
                       ret != BLE_STATUS_SUCCESS);
    }

    /* 10c: READ_CONFIG_DATA abnormal parameters */
    {
        uint8_t data[32];
        uint8_t data_len = 0;

        ret = aci_hal_read_config_data(0x01, &data_len, data);
        BT_TEST_ASSERT("read_config_data(offset=0x01) invalid offset => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_read_config_data(0x50, &data_len, data);
        BT_TEST_ASSERT("read_config_data(offset=0x50) invalid offset => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_read_config_data(0xFE, &data_len, data);
        BT_TEST_ASSERT("read_config_data(offset=0xFE) invalid offset => error",
                       ret != BLE_STATUS_SUCCESS);
    }

    /* 10d: EAD encrypt/decrypt abnormal parameters */
    {
        const uint8_t key[16] = {0};
        const uint8_t iv[8]   = {0};
        uint8_t buf[64];
        uint16_t out_len = 0;
        const uint8_t dummy[4] = {0xDE, 0xAD, 0xBE, 0xEF};

        ret = aci_hal_ead_encrypt_decrypt(2, key, iv, sizeof(dummy), dummy, &out_len, buf);
        BT_TEST_ASSERT("ead_encrypt_decrypt(mode=2) invalid mode => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_ead_encrypt_decrypt(0xFF, key, iv, sizeof(dummy), dummy, &out_len, buf);
        BT_TEST_ASSERT("ead_encrypt_decrypt(mode=0xFF) invalid mode => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_ead_encrypt_decrypt(1, key, iv, 4, dummy, &out_len, buf);
        BT_TEST_ASSERT("ead_decrypt(in_len=4) shorter than 9-byte overhead => error",
                       ret != BLE_STATUS_SUCCESS);
        ret = aci_hal_ead_encrypt_decrypt(1, key, iv, 0, dummy, &out_len, buf);
        BT_TEST_ASSERT("ead_decrypt(in_len=0) zero length => error",
                       ret != BLE_STATUS_SUCCESS);
    }

    /* 10e: SET_PERIPHERAL_LATENCY — valid: 0x00 / 0x01 */
    ret = aci_hal_set_peripheral_latency(2);
    BT_TEST_ASSERT("set_peripheral_latency(enable=2) out of range => error",
                   ret != BLE_STATUS_SUCCESS);
    ret = aci_hal_set_peripheral_latency(0xFF);
    BT_TEST_ASSERT("set_peripheral_latency(enable=0xFF) out of range => error",
                   ret != BLE_STATUS_SUCCESS);

    /* 10f: SET_RADIO_ACTIVITY_MASK with undefined bits */
    ret = aci_hal_set_radio_activity_mask(0x8000);
    BT_TEST_ASSERT("set_radio_activity_mask(undefined bit 0x8000) => error or SUCCESS",
                   ret == BLE_STATUS_INVALID_PARAMS || ret == BLE_STATUS_SUCCESS);

    /* --- Summary --- */
    printf("\n===== ACI HAL Self-Test Result: %d PASS / %d FAIL =====\n\n",
           bt_test_pass, bt_test_fail);
}

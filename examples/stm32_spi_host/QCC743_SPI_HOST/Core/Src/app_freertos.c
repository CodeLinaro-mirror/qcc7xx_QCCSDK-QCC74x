/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os2.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include "spi.h"
#include "spi_test.h"
#include "app_atmodule.h"
#include "dwt.h"
#include "stream_buffer.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId_t console_taskhandle;

StreamBufferHandle_t uart_strm_buffer;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

uint8_t uart_rxbyte;
extern UART_HandleTypeDef huart1;

static at_host_handle_t g_at_handle;

static int arg_parse(char *in, char *argv[])
{
    if (in == NULL || argv == NULL)
        return 0;

    int argc = 0;
    char *start = NULL;

    /* Traverse the input string */
    for (char *p = in; *p != '\0'; ++p) {
        if (!isspace((uint8_t)*p)) {
            if (start == NULL) {
                /* Start of a new word */
                start = p;
            }
        } else {
            if (start != NULL) {
                /* End of a word: Insert null terminator and store the pointer */
                *p = '\0';
                argv[argc++] = start;
                start = NULL;
            }
        }
    }

    /* Handle the last word (if any) */
    if (start != NULL) {
        argv[argc++] = start;
    }

    return argc;
}

static int do_ips(int argc, char *argv[])
{
	char *ip_addr = NULL;

	if (argc <= 1) {
		printf("Please input ip addr\r\n");
		return -1;
	}
	ip_addr = (char *)argv[1];
	//at_iperf_tcp_rx_start(g_at_handle, ip_addr, 5001);
	at_iperf_tcp_rx_fast_start(g_at_handle, ip_addr, 5001);
	return 0;
}

static int do_ipus(int argc, char *argv[])
{
	char *ip_addr = NULL;

	if (argc <= 1) {
		printf("Please input ip addr\r\n");
		return -1;
	}
	ip_addr = argv[1];
	//at_iperf_udp_rx_start(g_at_handle, ip_addr, 5001);
	at_iperf_udp_rx_fast_start(g_at_handle, ip_addr, 5001);
	return 0;
}

static int do_ipu(int argc, char *argv[])
{
	char *ip_addr = NULL;

	if (argc <= 1) {
		printf("Please input ip addr\r\n");
		return -1;
	}
	ip_addr = argv[1];
	at_iperf_udp_tx_start(g_at_handle, ip_addr, 5001);
	return 0;
}

static int do_ipc(int argc, char *argv[])
{
	char *ip_addr = NULL;

	if (argc <= 1) {
		printf("Please input ip addr\r\n");
		return -1;
	}
	ip_addr = argv[1];
	at_iperf_tcp_tx_start(g_at_handle, ip_addr, 5001);
	return 0;
}

static int do_at_iperf_stop(int argc, char *argv[])
{
	at_iperf_stop(g_at_handle, 0);
	return 0;
}

static int do_ota_start(int argc, char *argv[])
{
	char *ip_addr = NULL;

	if (argc <= 1) {
		printf("Please input ip addr\r\n");
		return -1;
	}
	ip_addr = argv[1];
	at_ota_start(g_at_handle, ip_addr, 3365);
	return 0;
}

static int do_ota_stop(int argc, char *argv[])
{
	at_ota_finish(g_at_handle);
	return 0;
}

static int do_spi_dump(int argc, char *argv[])
{
	spi_dump();
	return 0;
}

static int do_spi_show_tput(int argc, char *argv[])
{
	int on = 0;

	if (argc >= 2)
		on = !!atoi(argv[1]);

	if (on)
		spi_start_show_tput();
	else
		spi_stop_show_tput();
	return 0;
}

static int do_spi_write_test(int argc, char *argv[])
{
	int count = 1, len = 2048, mode = 0;

	if (argc >= 2)
		mode = atoi(argv[1]);

	if (argc >= 3) {
		count = atoi(argv[2]);
		if (!count)
			count = 1;
	}

	if (argc >= 4) {
		len = atoi(argv[3]);
		if (!len)
			len = 2048;
	}

	spi_write_test(mode, count, len);
	return 0;
}

static int do_spi_read_test(int argc, char *argv[])
{
	int count = 1, len = 2048, mode = 0;

	if (argc >= 2)
		mode = atoi(argv[1]);

	if (argc >= 3) {
		count = atoi(argv[2]);
		if (!count)
			count = 1;
	}

	if (argc >= 4) {
		len = atoi(argv[3]);
		if (!len)
			len = 2048;
	}

	spi_read_test(mode, count, len);
	return 0;
}

static int do_spi_rx_perf(int argc, char *argv[])
{
	int on;

	if (argc < 2) {
		printf("Usage: spi_rx_perf <0|1>\r\n");
		return -2;
	}

	on = !!atoi(argv[1]);
	if (on)
		spi_start_rx_perf();
	else
		spi_stop_rx_perf();

	return 0;
}

static int do_spi_tx_perf(int argc, char *argv[])
{
	int on;

	if (argc < 2) {
		printf("Usage: spi_tx_perf <0|1>\r\n");
		return -2;
	}

	on = !!atoi(argv[1]);
	if (on)
		spi_start_tx_perf();
	else
		spi_stop_tx_perf();
	return 0;
}

int do_ps(int argc, char *argv[])
{
    int interval_ms = 500;
    int iterations = 1;

    if (argc >= 2) {
        interval_ms = atoi(argv[1]);
        if (interval_ms <= 0) {
            printf("Invalid interval, must be > 0\n");
            return -1;
        }
    }
    if (argc >= 3) {
        iterations = atoi(argv[2]);
        if (iterations <= 0) {
            printf("Invalid iteration count, must be > 0\n");
            return -1;
        }
    }

    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    if (task_count == 0) {
        printf("No tasks found.\n");
        return -1;
    }

    TaskStatus_t *task_status_prev = (TaskStatus_t *)pvPortMalloc(task_count * sizeof(TaskStatus_t));
    if (task_status_prev == NULL) {
        printf("Memory allocation failed for task_status_prev.\n");
        return -1;
    }

    TaskStatus_t *task_status_curr = (TaskStatus_t *)pvPortMalloc(task_count * sizeof(TaskStatus_t));
    if (task_status_curr == NULL) {
        printf("Memory allocation failed for task_status_curr.\n");
        vPortFree(task_status_prev);
        return -1;
    }

    uint32_t total_runtime_prev = 0, total_runtime_curr = 0;
    /* Sample at least once. */
    uxTaskGetSystemState(task_status_prev, task_count, (size_t *)&total_runtime_prev);

    for (int it = 0; it < iterations; ++it) {
        vTaskDelay(pdMS_TO_TICKS(interval_ms));

        uxTaskGetSystemState(task_status_curr, task_count, (size_t *)&total_runtime_curr);

        int32_t total_runtime_diff = (int32_t)(total_runtime_curr - total_runtime_prev);
        if (total_runtime_diff == 0)
            total_runtime_diff = 1;

        /* Print header. */
        printf("\r\n\r\n %-16s %-8s %-10s %-10s %-10s\n",
               "Task Name", "State", "Priority", "Stack", "CPU%");

        for (UBaseType_t i = 0; i < task_count; ++i) {
            TaskStatus_t *curr_task = &task_status_curr[i];

            /* Since the task list may have changed, look for the match. */
            TaskStatus_t *prev_task = NULL;
            for (UBaseType_t j = 0; j < task_count; ++j) {
                if (task_status_prev[j].xHandle == curr_task->xHandle) {
                    prev_task = &task_status_prev[j];
                    break;
                }
            }

            /* This is a new task. */
            if (!prev_task)
                continue;

            int32_t task_runtime_diff = (int32_t)(curr_task->ulRunTimeCounter - prev_task->ulRunTimeCounter);
            float cpu_usage = ((float)task_runtime_diff / total_runtime_diff) * 100.0;

            /* Print the task statistics. */
            printf(" %-16s %-8s %-2lu/%-8lu %-10u %-10.2f\n",
                   curr_task->pcTaskName,
                   (curr_task->eCurrentState == eRunning) ? "Running" :
                   (curr_task->eCurrentState == eReady) ? "Ready" :
                   (curr_task->eCurrentState == eBlocked) ? "Blocked" :
                   (curr_task->eCurrentState == eSuspended) ? "Suspended" :
                   (curr_task->eCurrentState == eDeleted) ? "Deleted" : "Unknown",
                   curr_task->uxBasePriority, curr_task->uxCurrentPriority,
                   curr_task->usStackHighWaterMark,
                   cpu_usage);
        }

        memcpy(task_status_prev, task_status_curr, task_count * sizeof(TaskStatus_t));
        total_runtime_prev = total_runtime_curr;
    }

    vPortFree(task_status_prev);
    vPortFree(task_status_curr);

    return 0;
}

struct cmd_entry {
	const char *name;
	const char *desc;
	int (*func)(int argc, char *argv[]);
};

static int do_help(int argc, char *argv[]);

static const struct cmd_entry cmds[] = {
	{"help", "Show help menu", do_help},
	{"ips", "", do_ips},
	{"ipus", "", do_ipus},
	{"ipc", "", do_ipc},
	{"ipu", "", do_ipu},
	{"iperf_stop", "", do_at_iperf_stop},
	{"ota_start", "", do_ota_start},
	{"ota_stop", "", do_ota_stop},
	{"spi_dump", "Dump SPI transaction details", do_spi_dump},
	{"spi_show_tput", "Start/Stop SPI throuput show", do_spi_show_tput},
	{"spi_write_test", "SPI write test <mode 0/1 | packets | length>, e.g. spi_write_test 1 10", do_spi_write_test},
	{"spi_read_test", "SPI read test <mode 0/1 | packets | length>, e.g. spi_read_test 1 10", do_spi_read_test},
	{"spi_tx_perf", "Start/Stop SPI TX performance, spi_tx_perf <0 | 1>", do_spi_tx_perf},
	{"spi_rx_perf", "Start/Stop SPI RX performance, spi_rx_perf <0 | 1>", do_spi_rx_perf},
	{"ps", "Report information of the current processes, ps [interval_ms] [counter], ps 200 2", do_ps},
};

static int do_help(int argc, char *argv[])
{
	int i;

	for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
		printf("%-20s %s\r\n", cmds[i].name, cmds[i].desc);
	}
	return 0;
}

#define CONSOLE_AT_ENABLE 	1
#define CONSOLE_CMD_MAX_LEN	512
#define CONSOLE_CMD_MAX_ARGS 64
#define CONSOLE_CMD_TASK_STACK_SIZE	(8 * 1024)
#define HOSTCMD_KEYWORD "HOSTCMD"

struct console_task_info {
	const struct cmd_entry *entry;
	char cmd[CONSOLE_CMD_MAX_LEN];
};

static void console_cmd_task_entry(void *arg)
{
	int off = 0, argc;
	char *argv[CONSOLE_CMD_MAX_ARGS];
	struct console_task_info *info = arg;

	if (!info) {
		printf("console task info is NULL?!\r\n");
		goto out;
	}

	/* Re-parse... */
	argc = arg_parse(info->cmd, argv);
	if (argc <= 2) {
		printf("The command is supposed to be {command} xxxx &\r\n");
		goto out;
	}

	/* Remove the leading HOSTCMD keyword. */
	if (!strncmp(argv[0], HOSTCMD_KEYWORD, strlen(HOSTCMD_KEYWORD))) {
		argc--;
		off++;
	}

	/* Remove the terminating & */
	argc--;

	if (info->entry && info->entry->func)
		info->entry->func(argc, &argv[off]);

out:
	printf("command task %s exiting\r\n", info->entry->name);
	vPortFree(info);
	osThreadExit();
}

static int cli_handle_one(const char *cmdstr, int argc, char *argv[])
{
	int spawn = 0;
	int cmd_len, i, ret = -2;

	if (argc <= 0) {
    	printf("Invalid shell input\r\n");
    	return -1;
    }

	for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
		const struct cmd_entry *cmd = &cmds[i];

        cmd_len = strlen(cmd->name);
		if (!strncmp(argv[0], cmd->name, cmd_len)) {
			printf("executing command %s\r\n", argv[0]);
			if (argc >= 2)
				spawn = !strncmp(argv[argc - 1], "&", 1);

			printf("spawn %d\r\n", spawn);
			if (!spawn) {
				if (cmd->func)
					ret = cmd->func(argc, argv);
			} else {
				osThreadId_t task_handle;
				struct console_task_info *info;

				osThreadAttr_t task_attr = {
				  .name = cmd->name,
				  .priority = (osPriority_t) osPriorityRealtime6,
				  .stack_size = CONSOLE_CMD_TASK_STACK_SIZE,
				};

				info = pvPortMalloc(sizeof(struct console_task_info));
				if (!info) {
					printf("No mem for new console task info\r\n");
					break;
				}

				info->entry = cmd;
				strncpy(info->cmd, cmdstr, sizeof(info->cmd) - 1);

				task_handle = osThreadNew(console_cmd_task_entry, info, &task_attr);
				if (!task_handle) {
					printf("Failed to spawn new thread for command %s\r\n", cmd->name);
					vPortFree(info);
					ret = -3;
				}
				ret = 0;
			}
			break;
		}
	}

	if (ret == -2) {
		printf("Unknown command '%s' input '%s'\r\n", argv[0], cmdstr);
	}

	return ret;
}

static int _console_to_at(const char *buf, uint32_t len)
{
    int ret = at_host_send(g_at_handle, 0, (uint8_t *)buf, len, 0);

    if (ret < len) {
        printf("%s send fail\r\n", __func__);
    }
    return 0;
}

static int console_cli_run(const char *cmd)
{
	int argc;
	char *argv[CONSOLE_CMD_MAX_ARGS];
	char copy[CONSOLE_CMD_MAX_LEN];

	strncpy(copy, cmd, sizeof(copy) - 1);
	argc = arg_parse(copy, argv);
	if (argc <= 0)
		goto err_out;

#if CONSOLE_AT_ENABLE
	if (!strncmp(HOSTCMD_KEYWORD, argv[0], strlen(HOSTCMD_KEYWORD))) {
		if (argc > 1) {
			return cli_handle_one(cmd, argc - 1, &argv[1]);
		}

		goto err_out;
	}

	return _console_to_at(cmd, strlen(cmd));
#else
	return cli_handle_one(cmd, argc, argv);
#endif

err_out:
	printf("Invalid console input '%s'\r\n", copy);
	return -1;
}

static void uart_console_task(void *param)
{
	enum {
		CLI_INPUT_STATE_COMMON,
		CLI_INPUT_STATE_CR,
	} input_state;

	size_t ret;
	char saved;
	char buf[CONSOLE_CMD_MAX_LEN] = {0};
	HAL_StatusTypeDef status;
	unsigned int in = 0, out = 0;
	input_state = CLI_INPUT_STATE_COMMON;

#define BUF_SPACE()	(sizeof(buf) - in)
#define BUF_EMPTY()	(in == out)

	status = HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart_rxbyte, 1);
	if (status != HAL_OK)
		printf("failed to setup uart reception, %d\r\n", status);

	while (1) {
		ret = xStreamBufferReceive(uart_strm_buffer, &buf[in],
								BUF_SPACE(), portMAX_DELAY);
		if (ret) {
			/* new data arrives */
			in += ret;

            if (strstr(buf, "+++") != NULL) {
                _console_to_at(buf, 3);
                out = 3;
                in -= out;
            }
			/* parse received data */
			while (in > out) {
				char ch = buf[out++];

				switch (input_state) {
				case CLI_INPUT_STATE_COMMON:
					if (ch == '\r')
						input_state = CLI_INPUT_STATE_CR;
					break;

				case CLI_INPUT_STATE_CR:
					if (ch == '\n') {
						/*
						 * To assure that a command always has a terminating '\0',
						 * save the pending character, replace it with '\0' and resume
						 * it later.
						 */
						saved = buf[out];
						buf[out] = '\0';

						/* Removing possible leading NULL data. */
						int i;
						for (i = 0; !buf[i] && i < out; i++);

						console_cli_run(&buf[i]);

						/* Resume the input buffer. */
						buf[out] = saved;
						/* Shift out the old command. */
						memmove(buf, &buf[out], in - out);
						in -= out;
						out = 0;
					}
					input_state = CLI_INPUT_STATE_COMMON;
					break;
				}
			}
		}
	}
}

extern UART_HandleTypeDef huart1;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	HAL_StatusTypeDef status;
	BaseType_t pxHigherPriorityTaskWoken = 0;

	if (huart->Instance == USART1)
	{
		if (uart_strm_buffer) {
			xStreamBufferSendFromISR(uart_strm_buffer,
							&uart_rxbyte, 1, &pxHigherPriorityTaskWoken);
			//portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
		}
	}
	/* set up next uart reception */
	status = HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart_rxbyte, 1);
	if (status != HAL_OK)
		printf("failed to setup next uart reception, %d\r\n", status);
}

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void configureTimerForRunTimeStats(void);
unsigned long getRunTimeCounterValue(void);

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{
	dwt_enable();
	dwt_cycle_counter_enable();
	dwt_cycle_counter_reset();
}

__weak unsigned long getRunTimeCounterValue(void)
{
	return dwt_cycle_counter_read();
}

void trace_task_switched_out(void)
{
	//itm_printf("switch out %s", pcTaskGetName(NULL));
}

void trace_task_switched_in(void)
{
	//itm_printf("switch in %s", pcTaskGetName(NULL));
}

/* USER CODE END 1 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
	int ret;

	osThreadAttr_t defaultTask_attributes = {
	  .name = "defaultTask",
	  .priority = (osPriority_t) osPriorityNormal,
	  .stack_size = 1024 * 1,
	};
	osThreadAttr_t console_tsk_attr = {
	  .name = "console",
	  .priority = (osPriority_t) osPriorityRealtime6,
	  .stack_size = 1024 * 4,
	};

	uart_strm_buffer = xStreamBufferCreate(1024 * 3, 1);
	if (!uart_strm_buffer)
		printf("failed to create stream buffer for uart\r\n");
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  console_taskhandle = osThreadNew(uart_console_task, NULL, &console_tsk_attr);
  if (!console_taskhandle) {
	  printf("failed to create console task\r\n");
	  while (1);
  }

  ret = spi_transaction_init();
  if (ret) {
	  printf("failed to init spi transaction, %d\r\n", ret);
	  while (1);
  }

  g_at_handle = at_spisync_init();

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartDefaultTask */
/**
* @brief Function implementing the defaultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN defaultTask */
  /* Infinite loop */

  for(;;)
  {
	  HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
	  HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
	  HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
	  osDelay(500);
  }
  /* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */


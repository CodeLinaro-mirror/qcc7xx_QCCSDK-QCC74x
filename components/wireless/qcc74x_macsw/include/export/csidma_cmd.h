#ifndef _CISDMA_CMD_H_
#define _CISDMA_CMD_H_

/**
 * @file csidma_cmd.h
 * @brief CLI/AT command entries for the WiFi CSI (Channel State Information)
 *        DMA driver.
 *
 * The CSI DMA engine captures per-packet channel estimation data (CSI) for a
 * given peer MAC address into one of CSI_BUF_NUM hardware buffers. Each command
 * below follows the shell convention: it receives the tokenized command line as
 * (argc, argv) and parses its own options with utils_getopt(). The underlying
 * driver primitives are declared in csidma.h.
 */

/**
 * @brief Start a CSI DMA capture on one buffer.
 *
 * Lazily registers the CSI DMA interrupt handler, creates the CSI worker task
 * (decodes raw buffers and prints "CSI: ..." reports) on first use, then arms
 * the requested buffer.
 *
 * Options (getopt style):
 *   -a <mac>  Target peer MAC address, format "00:11:22:33:44:55" (mandatory).
 *   -b <idx>  Buffer index, range [0, CSI_BUF_NUM-1].
 *   -m <mode> CSI mode: 0 legacy LTF, 1 HT LTF, 2 auto.
 *             (Only honored when WIFI_CSI_FULL_FEATURE is defined; otherwise
 *              forced to 0.)
 *   -t <mode> Time control mode: 0 off, 1 one-shot, 2 periodical, 3 FTM trigger.
 *             (Only honored when WIFI_CSI_FULL_FEATURE is defined; otherwise
 *              forced to 2.)
 *   -i <ms>   Trigger timestamp / interval in milliseconds.
 *   -r <0|1>  Buffer ready policy after the ISR: 0 keep the buffer pending
 *             (DMA will not overwrite it until it is marked ready again),
 *             1 always mark the buffer ready after the ISR.
 *   -h        Print the help menu.
 *
 * @param argc Argument count.
 * @param argv Argument vector (argv[0] is the command name).
 */
void cmd_csidma_start(int argc, char **argv);

/**
 * @brief Stop CSI DMA capture and release resources.
 *
 * Usage: csidma_stop [buffer index]
 *   With a buffer index, stops only that buffer.
 *   Without an argument, stops all CSI_BUF_NUM buffers.
 * When the last active buffer is stopped, the CSI worker task, message buffer
 * and semaphore are torn down.
 *
 * @param argc Argument count.
 * @param argv Argument vector; argv[1] is the optional buffer index.
 */
void cmd_csidma_stop(int argc, char **argv);

/**
 * @brief Re-arm a buffer left pending after its ISR.
 *
 * Usage: csidma_ready <buffer index>
 * Only meaningful when the buffer was started with the "keep pending" policy
 * (csidma_start -r 0): marks the buffer ready again so the DMA engine may
 * overwrite it on the next capture.
 *
 * @param argc Argument count (must be > 1).
 * @param argv Argument vector; argv[1] is the buffer index.
 */
void cmd_csidma_ready(int argc, char **argv);

/**
 * @brief Enable or disable CSI DMA driver debug output.
 *
 * Usage: csidma_debug <0|1>
 *
 * @param argc Argument count (must be > 1).
 * @param argv Argument vector; argv[1] is 0 (off) or 1 (on).
 */
void cmd_csidma_debug(int argc, char **argv);

/**
 * @brief Force the CSI/FTM hardware trigger on or off (test only).
 *
 * Usage: csidma_force_ftm [0|1]
 * Writes the CSI/FTM control register directly to force the FTM-based capture
 * trigger. Compiled in only when CSIDMA_FTM_ENABLE is defined; intended for
 * FTM bring-up testing.
 *
 * @param argc Argument count.
 * @param argv Argument vector; argv[1] selects enable (non-zero) / disable (0).
 */
void cmd_csidma_force_ftm(int argc, char **argv);

/**
 * @brief Override the FTM peer MAC address used by the forced FTM trigger
 *        (test only).
 *
 * Usage: csidma_force_ftm_mac_set -h <high_mac_addr> -l <low_mac_addr>
 * Both values are hexadecimal. Compiled in only when CSIDMA_FTM_ENABLE is
 * defined; intended for FTM bring-up testing.
 *
 * @param argc Argument count.
 * @param argv Argument vector (getopt: -h high half, -l low half).
 */
void cmd_csidma_force_ftm_mac_set(int argc, char **argv);

/*
 * Programmatic CSI capture API.
 *
 * Unlike the cmd_csidma_*() shell entries above (which decode the CSI report
 * and print it to the local console), this API lets any caller receive every
 * decoded CSI report through a callback as a ready-to-emit formatted string,
 * so the data can be routed to the caller's own output channel.
 */

#include <stdint.h>

/**
 * @brief CSI capture configuration passed to csidma_capture_start().
 */
struct csidma_capture_cfg {
    /** Target peer MAC address (filter); must not be all zero. */
    uint8_t  mac_addr[6];
    /** Buffer index, range [0, CSI_BUF_NUM-1]. */
    uint8_t  buf_idx;
    /** CSI mode: 0 legacy LTF, 1 HT LTF, 2 auto (honored on full-feature). */
    uint8_t  csi_mode;
    /** Time control mode: 0 off, 1 one-shot, 2 periodical, 3 FTM trigger. */
    uint8_t  ts_mode;
    /** Trigger timestamp / interval in milliseconds. */
    uint32_t trig_itvl_ms;
    /** Buffer ready policy after ISR: 0 keep pending, 1 always ready. */
    uint8_t  ready_after_isr;
};

/**
 * @brief Callback delivering one decoded CSI report as a formatted string.
 *
 * Invoked from the CSI worker task context (not ISR). The string holds the
 * report fields (timestamp, port, mac, rssi, ..., CSI matrix) WITHOUT any
 * leading tag or trailing newline, so the caller is free to wrap it.
 *
 * @param str   Formatted CSI report (no tag, no trailing newline).
 * @param len   Length of str in bytes.
 * @param arg   Opaque user data passed to csidma_capture_start().
 */
typedef void (*csidma_report_cb)(const char *str, int len, void *arg);

/**
 * @brief Start CSI capture and deliver decoded reports via callback.
 *
 * Lazily registers the CSI interrupt handler and creates the worker task on
 * first use. Each captured report is decoded and handed to @p cb as a string.
 *
 * @param cfg  Capture configuration.
 * @param cb   Report callback (must not be NULL).
 * @param arg  Opaque user data forwarded to @p cb.
 * @return 0 on success, negative on error (invalid cfg / no resources).
 */
int csidma_capture_start(const struct csidma_capture_cfg *cfg, csidma_report_cb cb, void *arg);

/**
 * @brief Stop CSI capture started with csidma_capture_start().
 *
 * @param buf_idx Buffer index to stop, or -1 to stop all buffers and tear
 *                down the worker task.
 */
void csidma_capture_stop(int buf_idx);

#endif

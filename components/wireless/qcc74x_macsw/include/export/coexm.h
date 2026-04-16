/**
 * @file coexm.h
 * @brief Unified export header for coexistence module
 *
 * This header provides access to all coex-related APIs including:
 * - Coex protection APIs (coex_protect_acquire/release)
 * - PM coex functions (pm_coex_init/deinit, etc.)
 *
 * This is the single header that external modules should include for
 * coex functionality. The previous coex_protect.h has been merged here.
 */

#ifndef _EXPORT_COEXM_H_
#define _EXPORT_COEXM_H_

/**
 ****************************************************************************************
 * @defgroup PM
 * @ingroup PM
 * @brief power management
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DEFINITIONS
 ****************************************************************************************
 */

/**
 * @brief PTA (Packet Traffic Arbiter) role definitions
 *
 * Defines the different operating modes for the PTA hardware which controls
 * RF access arbitration between WiFi, BLE, Thread, and other protocols.
 */
enum pta_role {
    PTA_ROLE_BT = 0,                    ///< BLE exclusive mode (BLE has priority)
    PTA_ROLE_WIFI,                      ///< WiFi exclusive mode (WiFi has priority)
    PTA_ROLE_WIFI_AND_BT_DEFAULT,       ///< Compete mode (WiFi and BLE use default priority)
    PTA_ROLE_THREAD,                    ///< Thread exclusive mode
    PTA_ROLE_PTI,                       ///< PTI (Packet Traffic Information) mode
    PTA_ROLE_MAX,                       ///< MAX role
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

typedef struct {
    bool pta_force_enable;
} pm_coex_init_params_t;

/// TXQ pending query callback (implemented by fhost, registered at runtime)
typedef bool (*coex_txq_has_pending_cb_t)(uint8_t vif_idx);

/*
 * pm_coex_sleep() failure bitmask (Stage 1)
 ****************************************************************************************
 */

#define PM_COEX_SLEEP_FAIL_GUARD         (1u << 0)
#define PM_COEX_SLEEP_FAIL_PS_OFF        (1u << 1)
#define PM_COEX_SLEEP_FAIL_PREVENT_SLEEP (1u << 2)
#define PM_COEX_SLEEP_FAIL_TX_INFLIGHT   (1u << 3)
#define PM_COEX_SLEEP_FAIL_HW_TIMER      (1u << 4)
#define PM_COEX_SLEEP_FAIL_KE_MSG        (1u << 5)
#define PM_COEX_SLEEP_FAIL_CPU           (1u << 6)
#define PM_COEX_SLEEP_FAIL_SLEEP_CTL     (1u << 7)

/**
 * @brief Initialize the PM COEX module.
 * @param params Initialization parameters (must not be NULL).
 * @return 0 on success, -1 on failure.
 */
int pm_coex_init(const pm_coex_init_params_t *params);

/**
 * @brief De-initialize the PM COEX module.
 * @note Restores the PTA state to its original configuration.
 * @return 0 on success, -1 on failure.
 */
int pm_coex_deinit(void);

/**
 * @brief Enter sleep mode.
 * @note Performs a pre-check to ensure the system can sleep safely.
 *
 * @return 0 on success. On failure returns a non-zero bitmask (see
 *         PM_COEX_SLEEP_FAIL_*).
 */
int pm_coex_sleep(void);

/**
 * @brief Wake the system from sleep.
 * @return 0 on success, -1 on failure.
 */
int pm_coex_wakeup(void);

/**
 * @brief Pause the power management functionality.
 * @details When paused, pm_coex_sleep and pm_coex_wakeup calls will be ignored.
 * @return 0 on success, -1 if PM not initialized.
 */
int pm_coex_pause(void);

/**
 * @brief Resume the power management functionality.
 * @details Allows pm_coex_sleep and pm_coex_wakeup calls to function normally.
 * @return 0 on success, -1 if PM not initialized.
 */
int pm_coex_resume(void);

/**
 * @brief Coex coordinator "gate" (persistent enable flag).
 * @return true if coexistence mode is enabled.
 */
bool coex_coord_is_enabled(void);

/**
 * @brief Coex coordinator runtime-active gate.
 * @return true if coexistence is enabled and runtime is active.
 */
bool coex_coord_is_active(void);

/**
 * @brief Backward-compatible alias of @ref coex_coord_is_enabled.
 * @deprecated Prefer @ref coex_coord_is_enabled.
 */
bool ps_is_coex_mode(void);

/**
 * @brief Enable WiFi/BLE coexistence mode (persistent).
 *
 * Upper-layer control path entrypoint. Enables coexistence at PS layer and
 * prepares coordinator/protection. Runtime may start immediately if Wi-Fi PS
 * is already ON.
 */
void ps_coex_mode_enable(void);

/**
 * @brief Disable WiFi/BLE coexistence mode.
 *
 * Upper-layer control path entrypoint. Disables runtime, clears PS flags and
 * restores default PTA behavior.
 */
void ps_coex_mode_disable(void);

/*
 * COEX coordinator - Stage 1 entrypoints
 ****************************************************************************************
 */

/**
 * @brief Register a callback for querying whether Host TXQ has pending packets.
 */
void coex_coord_register_txq_has_pending_cb(coex_txq_has_pending_cb_t cb);

/**
 * @brief TBTT hook (Wi-Fi RX anchor) for coex coordinator.
 * @param tbtt_time   TBTT timestamp (us, MAC timer).
 * @param vif_index   MAC VIF index associated with this TBTT.
 */
void coex_coord_on_tbtt(uint32_t tbtt_time, uint8_t vif_index);

/**
 * @brief Called by platform right before Wi-Fi task blocks.
 * @return true if `pm_coex_sleep()` commit succeeded.
 */
bool coex_coord_on_wifi_suspend_enter(void);

/**
 * @brief Called by platform right after Wi-Fi task is woken up.
 * @param slept_committed  Return value from @ref coex_coord_on_wifi_suspend_enter.
 */
void coex_coord_on_wifi_wake(bool slept_committed);

void coex_coord_on_enable(void);
void coex_coord_on_disable(void);
void coex_coord_on_runtime_pause(void);
void coex_coord_on_runtime_resume(void);

/**
 * @brief Check if currently in WiFi active window (WiFi's time slice).
 * @details Returns true if WiFi is in its allocated time slice and can use PTA_ROLE_WIFI.
 *          Returns false if outside WiFi's time slice (e.g., BLE's time slice), where WiFi
 *          should compete using PTA_ROLE_WIFI_AND_BT_DEFAULT instead.
 * @return true if in WiFi active window, false otherwise.
 */
bool pm_coex_is_wifi_active_window(void);

/**
 * @brief Set WiFi active window state (internal use by coex coordinator).
 * @param[in] is_active true if entering WiFi's time slice, false if leaving.
 * @return 0 on success, -1 if PM not initialized.
 *
 * @note This function should be called by the coex coordinator when:
 *       - Entering WiFi active window: TBTT (RX anchor)
 *       - Leaving WiFi active window: slice-end timer (or protect overrides)
 */
int pm_coex_set_wifi_active_window(bool is_active);

/**
 * @brief Set WiFi connecting state (internal use by SM module).
 * @param[in] is_connecting true if WiFi is connecting, false if connected or idle.
 * @return 0 on success, -1 if PM not initialized.
 *
 * @note This function should be called by SM module when:
 *       - Entering connecting state: when starting authentication/association
 *       - Leaving connecting state: when connection completes or fails
 *       During connection, PTA is forced to PTA_ROLE_WIFI for exclusive RF access.
 */
int pm_coex_set_wifi_connecting(bool is_connecting);

void pm_coex_force_wifi_role(void);
void pm_coex_force_ble_and_thread(void);
void pm_coex_set_wifi_ack_ba_autoresp_block(bool disable);

/// Aggregated debug/status snapshot for CLI/diagnostics.
struct pm_coex_status {
    uint8_t ps_coex_state;
    uint8_t pm_state;
    uint8_t pta_current_role;
    bool wifi_active_window;
    bool wifi_connecting;
    uint32_t wifi_duty_ms;
    bool protect_enabled;
    bool protect_active;
    uint32_t protect_mask;
};

int pm_coex_get_status(struct pm_coex_status *out);

/*
 * COEX PROTECT DEFINITIONS (merged from coex_protect.h)
 ****************************************************************************************
 */

/**
 * @brief Protection types for WiFi operations
 *
 * Each type represents a logical WiFi operation that requires RF protection.
 * Multiple types can be active simultaneously (reference counted).
 */
typedef enum {
    COEX_PROT_SCAN = 0,         ///< WiFi scanning
    COEX_PROT_CONNECT,          ///< Connection (Auth + Assoc + Handshake)
    COEX_PROT_DHCP,             ///< DHCP address acquisition
    COEX_PROT_DISCONNECT,       ///< Disconnection processing
    COEX_PROT_KEY_MGMT,         ///< Key management (4-way handshake)
    COEX_PROT_MAX
} coex_protect_type_t;

/*
 * COEX PROTECT FUNCTION DECLARATIONS
 ****************************************************************************************
 */

#if NX_COEX_POWERSAVE

/**
 * @brief Enable or disable coex protection module
 *
 * When disabled, acquire/release calls become no-ops (return success).
 * Should be called by BLE or other RF-sharing modules during init/deinit.
 *
 * Default state: DISABLED (no overhead for pure WiFi scenarios)
 *
 * @param enable  true to enable, false to disable
 */
void coex_protect_set_enabled(bool enable);

/**
 * @brief Check if coex protection module is enabled
 *
 * @return true if enabled, false otherwise
 */
bool coex_protect_is_enabled(void);

/**
 * @brief Acquire RF protection for a specific operation type
 *
 * If module is disabled, returns success immediately (no-op).
 * Increments reference count for the specified type.
 * When total count goes from 0 to 1, enables RF protection.
 *
 * @param type The operation type requiring protection
 * @return 0 on success, -1 on error
 */
int coex_protect_acquire(coex_protect_type_t type);

/**
 * @brief Release RF protection for a specific operation type
 *
 * Decrements reference count for the specified type.
 * When total count goes from 1 to 0, disables RF protection.
 *
 * @param type The operation type releasing protection
 * @return 0 on success, -1 on error (e.g., release without acquire)
 */
int coex_protect_release(coex_protect_type_t type);

/**
 * @brief Force release all protection (emergency cleanup)
 *
 * Clears all reference counts and disables protection.
 * Use only in error recovery paths (connection failure, timeout, disconnect).
 */
void coex_protect_release_all(void);

/**
 * @brief Check if any protection is currently active
 * @return true if protection is active
 */
bool coex_protect_is_active(void);

/**
 * @brief Dump current protection state (for debugging)
 */
void coex_protect_dump(void);

/**
 * @brief Get current protection mask (bit per @ref coex_protect_type_t).
 */
uint32_t coex_protect_get_mask(void);

#else /* !NX_COEX_POWERSAVE */

/*
 * No-op stubs when coex powersave is disabled
 */
static inline void coex_protect_set_enabled(bool enable) { (void)enable; }
static inline bool coex_protect_is_enabled(void) { return false; }

static inline int coex_protect_acquire(coex_protect_type_t type)
{
    (void)type;
    return 0;
}

static inline int coex_protect_release(coex_protect_type_t type)
{
    (void)type;
    return 0;
}

static inline void coex_protect_release_all(void) {}
static inline bool coex_protect_is_active(void) { return false; }
static inline void coex_protect_dump(void) {}
static inline uint32_t coex_protect_get_mask(void) { return 0; }

#endif /* NX_COEX_POWERSAVE */

/**
 * @brief 4-way handshake wait timeout (5 seconds)
 *
 * Fallback protection for Open networks, PSK errors, and AP delays.
 * Normal M1 arrival: 100-500ms.
 */
#ifndef PM_COEX_HANDSHAKE_WAIT_TIMEOUT_MS
#define PM_COEX_HANDSHAKE_WAIT_TIMEOUT_MS    10000
#endif

/**
 * @brief Enable debug logging for COEX RF protection
 *
 * 0 = Disabled (production builds) - No protection logs
 * 1 = Enabled (debug builds) - Verbose protection state logs
 */
#ifndef COEX_PROTECT_DEBUG_LOG
#define COEX_PROTECT_DEBUG_LOG    1
#endif

#if COEX_PROTECT_DEBUG_LOG
#define COEX_LOG(fmt, ...) qcc74x_fw_printf("[COEX] " fmt, ##__VA_ARGS__)
#else
#define COEX_LOG(fmt, ...) do {} while(0)
#endif


#ifdef __cplusplus
}
#endif


#endif /* _EXPORT_COEXM_H_ */

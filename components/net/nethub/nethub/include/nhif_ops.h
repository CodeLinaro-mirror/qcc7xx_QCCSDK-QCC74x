/**
 * @file nhif_ops.h
 * @brief nethub interface operation abstraction
 */

#ifndef _NHIF_OPS_H
#define _NHIF_OPS_H

#include "nethub_types.h"

/* Forward declaration */
struct nhif_ops;

/**
 * @brief Network interface instance structure
 */
typedef struct nhif_instance {
    nhif_type_t type;                          /**< Interface type */
    nhif_state_t state;                        /**< Interface state */
    const struct nhif_ops *ops;                /**< Interface operation callbacks */
    void *private_data;                        /**< Interface private data */
} nhif_instance_t;

/**
 * @brief Network interface operation table
 * @note Each interface type must implement this structure
 */
struct nhif_ops {
    /**
     * @brief Interface type
     */
    nhif_type_t type;

    /**
     * @brief Initialize the interface
     * @return 0 on success, negative error code on failure
     */
    int (*init)(void);

    /**
     * @brief Deinitialize the interface
     * @return 0 on success, negative error code on failure
     */
    int (*deinit)(void);

    /**
     * @brief Input callback
     * @details Called when the interface receives data and passes it to nethub
     * @param skb packet buffer
     * @param arg user-defined argument
     * @return forwarding decision result
     */
    nhforward_callback_t input;

    /**
     * @brief Output callback
     * @details Called when nethub needs to send data through the interface
     * @param skb packet buffer
     * @param arg user-defined argument
     * @return forwarding decision result
     */
    nhforward_callback_t output;
};

/**
 * @brief Register a network interface
 * @param ops interface operation table
 * @param private_data private data
 * @return 0 on success, negative error code on failure
 */
int nhif_register(const struct nhif_ops *ops, void *private_data);

/**
 * @brief Unregister a network interface
 * @param type interface type
 * @return 0 on success, negative error code on failure
 */
int nhif_unregister(nhif_type_t type);

/**
 * @brief Find an interface instance by type
 * @param type interface type
 * @return interface instance pointer, or NULL if not found
 */
nhif_instance_t* nhif_find_by_type(nhif_type_t type);

/**
 * @brief Initialize all registered interfaces
 * @return 0 on success, negative error code on failure
 */
int nhif_init_all(void);

#endif /* _NHIF_OPS_H */

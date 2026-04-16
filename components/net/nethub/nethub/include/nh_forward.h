/**
 * @file nh_forward.h
 * @brief nethub forwarding rule configuration
 */

#ifndef _NH_FORWARD_H
#define _NH_FORWARD_H

#include "nethub_types.h"


/**
 * @brief Set the forwarding rule configuration
 * @param config forwarding rule configuration
 * @return 0 on success, negative error code on failure
 */
int nh_forward_set_config(const nh_forward_config_t *config);

/**
 * @brief Get the current forwarding rule configuration
 * @return pointer to the current forwarding rule configuration
 */
const nh_forward_config_t* nh_forward_get_config(void);

/**
 * @brief Execute packet forwarding
 * @param skb packet buffer
 * @param src_type source interface type
 * @return forwarding decision result
 */
nh_forward_result_t nh_forward_execute(nh_skb_t *skb, nhif_type_t src_type);

/**
 * @brief Get the forwarding rule table for the specified interface
 * @param src_type source interface type
 * @return forwarding rule table pointer, or NULL if not found
 */
const nh_if_forward_table_t* nh_forward_get_if_table(nhif_type_t src_type);

/**
 * @brief Print the current forwarding rule table
 */
void nh_forward_print_rules(void);

/**
 * @brief Validate the forwarding rule configuration
 * @param config forwarding rule configuration
 * @return 0 if valid, negative error code otherwise
 */
int nh_forward_validate_config(const nh_forward_config_t *config);

#endif /* _NH_FORWARD_H */

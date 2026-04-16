#ifndef __RCP_MCU_CAPABILITY_H__
#define __RCP_MCU_CAPABILITY_H__

#include <stdbool.h>

enum mcu_cap_type {
	RCP_MCU_CAP_WIFI, // Indicates both data and management
	RCP_MCU_CAP_BT,
	RCP_MCU_CAP_THREADS,
	RCP_MCU_CAP_ANY, // Used as wild card
};

bool get_mcu_capability();

bool is_capability_supported(enum mcu_cap_type);

#endif

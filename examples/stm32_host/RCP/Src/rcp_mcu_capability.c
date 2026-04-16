#include <rcp_cntrl_msgs.h>
#include <rcp_os.h>

#include "rcp_mcu_capability.h"
#include "log.h"

#define MCU_CAP_TIMEOUT_MS 1000

static rcp_sem_t mcu_cap_sem;
static bool mcu_cap_sem_ready;

static uint8_t mcu_capability;

void cntrl_msg_handler(uint8_t msg_type, uint8_t subtype, buffer_mgmt_elem_t *payload_ptr)
{
	if (msg_type == RCP_MSG_TYPE_STATUS_IND)
	{
		switch (subtype)
		{
			case RCP_CNTRL_CAP_IND:
				if ((payload_ptr) && payload_ptr->data_size > 0) {
					mcu_capability = *BUFFER_MGMT_DATA_PTR(payload_ptr);
				}
				(void)rcp_os_sem_give(&mcu_cap_sem);
				break;

			default:
				LOG_WRN("Status Indication %d\n", subtype);
				break;
		}
	} else {
		LOG_WRN("Unhandled control msg %d subtype %d\n", msg_type, subtype);
	}
}

bool get_mcu_capability()
{
	if (!mcu_cap_sem_ready && !rcp_os_sem_init(&mcu_cap_sem, 0, 1)) {
		return false;
	}
	mcu_cap_sem_ready = true;

	mcu_capability = 0;
	(void)rcp_os_sem_take(&mcu_cap_sem, 0);
	register_cntrl_msg_hdlr(cntrl_msg_handler);

	if (!send_capability_request()) {
		LOG_ERR("Failed to send capability request \n");
		return false;
	}

	if (!rcp_os_sem_take(&mcu_cap_sem, MCU_CAP_TIMEOUT_MS)) {
		LOG_ERR("Failed to read interface MAC from the device");
		return false;
	}
	return true;
}

bool is_capability_supported(enum mcu_cap_type capability)
{
	switch (capability) {
	case RCP_MCU_CAP_ANY:
		return true;
	case RCP_MCU_CAP_WIFI:
		return mcu_capability & RCP_WIFI_CAPABILITY_FLAG;
	case RCP_MCU_CAP_BT:
		return mcu_capability & RCP_BT_CAPABILITY_FLAG;
	case RCP_MCU_CAP_THREADS:
		return mcu_capability & RCP_THREAD_CAPABILITY_FLAG;
	}
	LOG_WRN("Unknown MCU capability = %d", capability);
	return false;
}

#ifndef __RCP_BASE_H__
#define __RCP_BASE_H__

#include <stdbool.h>

enum rcp_status {
	RCP_OK = 0,
	RCP_BUSY = 1,
	RCP_ERROR = 2,
	RCP_TIMEOUT = 3,
	RCP_IO_ERROR = 4,
	RCP_UNEXPECTED_RESPONSE = 5,
};

bool rcp_init(void);
void rcp_deinit(void);

enum rcp_status RCP_At(void);
enum rcp_status RCP_Reset(void);

#endif /* __RCP_BASE_H__ */

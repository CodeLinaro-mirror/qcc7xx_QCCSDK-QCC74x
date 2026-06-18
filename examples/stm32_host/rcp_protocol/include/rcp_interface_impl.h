#ifndef __RCP_INTERFACE_IMPL_H__
#define __RCP_INTERFACE_IMPL_H__

#include "buffer_mgmt_ctrl.h"
#include "rcp_interface.h"
#include "rcp_mux_dispatcher.h"
#include "rcp_protocol.h"
#include "rcp_tx_scheduler.h"
#include "rcp_bus.h"

void run_event_loop(enum rcp_bus bus);

/* Internal accessors for stat / dump modules. Avoid exposing file-scope
 * globals; rcp_stat.c uses these to print runtime state without owning it. */
struct rcp_dump_ctx {
	enum rcp_bus              bus;
	lnk_sts_t                *streams;          /* RCP_MSG_TYPE_MAX entries */
	rcp_tx_sch_t             *ctrl_sch;
	buffer_mgmt_ctrl_t       *cmd_buffer_ctrl;
	buffer_mgmt_ctrl_t       *rx_buffer_ctrl;
};

void rcp_internal_get_dump_ctx(struct rcp_dump_ctx *ctx);

#endif

#include <stddef.h>
#include <stdbool.h>
#include <rcp_bus.h>
#include <rcp_os.h>

struct bus_evt {
	rcp_event_t evt;
};

static struct {
	const struct rcp_bus_api *instances[RCP_BUS_MAX];
	struct bus_evt bus_evts[RCP_BUS_MAX];
} bus_ctx;

static const char *const status_string[] = {
	[RCP_BUS_OK] = "RCP_BUS_OK",
	[1] = "RCP_BUS_ERR_IN_PROGRESS",
	[2] = "RCP_BUS_ERR_NO_INST",
	[3] = "RCP_BUS_ERR_IO",
	[4] = "RCP_BUS_ERR_INVALID",
	[5] = "RCP_BUS_ERR_EMPTY",
	[6] = "RCP_BUS_ERR_CLOSED",
	[7] = "RCP_BUS_ERR_MEMORY",
	[8] = "RCP_BUS_ERR_PEER_MEMORY",
	[9] = "RCP_BUS_ERR_TIMEOUT",
};

static inline bool is_registered(enum rcp_bus bus)
{
	return (bus_ctx.instances[bus] != NULL);
}

static inline const struct rcp_bus_api *get_inst(enum rcp_bus bus)
{
	return bus_ctx.instances[bus];
}

static inline uint8_t get_bus_flags(enum rcp_bus bus)
{
	return (uint8_t)rcp_os_event_get_bits(&bus_ctx.bus_evts[bus].evt);
}

enum rcp_bus_status rcp_bus_register(enum rcp_bus bus, struct rcp_bus_api *api)
{
	if (bus >= RCP_BUS_MAX || api == NULL) {
		return RCP_BUS_ERR_INVALID;
	}
	if (is_registered(bus)) {
		return RCP_BUS_ERR_MEMORY;
	}

	if (api->open == NULL || api->write == NULL || api->close == NULL || api->read == NULL ||
	    api->get_caps == NULL || api->get_rx_len == NULL) {
		return RCP_BUS_ERR_INVALID;
	}

	if (!rcp_os_event_init(&bus_ctx.bus_evts[bus].evt)) {
		return RCP_BUS_ERR_MEMORY;
	}

	bus_ctx.instances[bus] = api;
	return RCP_BUS_OK;
}

enum rcp_bus_status rcp_bus_open(enum rcp_bus bus)
{
	if (!is_registered(bus)) {
		return RCP_BUS_ERR_INVALID;
	}

	return get_inst(bus)->open();
}

enum rcp_bus_status rcp_bus_close(enum rcp_bus bus)
{
	if (!is_registered(bus)) {
		return RCP_BUS_ERR_INVALID;
	}

	return get_inst(bus)->close();
}

enum rcp_bus_status rcp_bus_write(enum rcp_bus bus, rcp_msg_packt_t *packt, uint16_t buf_len)
{
	if (!is_registered(bus) || packt == NULL) {
		return RCP_BUS_ERR_INVALID;
	}

	return get_inst(bus)->write(packt, buf_len);
}

int rcp_bus_read(enum rcp_bus bus, rcp_msg_packt_t *packt, uint16_t buf_len)
{
	if (!is_registered(bus) || packt == NULL) {
		return RCP_BUS_ERR_INVALID;
	}

	return get_inst(bus)->read(packt, buf_len);
}

enum rcp_bus_status rcp_bus_get_caps(enum rcp_bus bus, struct rcp_bus_caps *caps)
{
	if (!is_registered(bus) || caps == NULL) {
		return RCP_BUS_ERR_INVALID;
	}

	return get_inst(bus)->get_caps(caps);
}

enum rcp_bus_status rcp_bus_wait_for_flags(enum rcp_bus bus, uint8_t flags, uint32_t timeout)
{
	if (bus >= RCP_BUS_MAX || !is_registered(bus) || flags == 0) {
		return RCP_BUS_ERR_INVALID;
	}

	return rcp_os_event_wait_bits(&bus_ctx.bus_evts[bus].evt, flags, true, timeout, NULL) ?
		RCP_BUS_OK : RCP_BUS_ERR_TIMEOUT;
}

enum rcp_bus_status rcp_bus_wait_for_any_flags(enum rcp_bus bus, uint8_t flags, uint32_t timeout,
					       uint8_t *matched_flags)
{
	uint32_t matched = 0;

	if (bus >= RCP_BUS_MAX || !is_registered(bus) || flags == 0) {
		return RCP_BUS_ERR_INVALID;
	}

	if (!rcp_os_event_wait_bits(&bus_ctx.bus_evts[bus].evt, flags, false, timeout, &matched)) {
		if (matched_flags) {
			*matched_flags = 0;
		}
		return RCP_BUS_ERR_TIMEOUT;
	}

	if (matched_flags) {
		*matched_flags = (uint8_t)matched;
	}
	return RCP_BUS_OK;
}

enum rcp_bus_status rcp_bus_wait(enum rcp_bus bus, uint32_t timeout_ms)
{
	if (bus >= RCP_BUS_MAX || !is_registered(bus)) {
		return RCP_BUS_ERR_INVALID;
	}

	return rcp_bus_wait_for_any_flags(bus, RCP_BUS_FLAG_RX_DATA | RCP_BUS_FLAG_TX_PENDING,
					  timeout_ms, NULL);
}

void rcp_bus_signal_tx_pending(enum rcp_bus bus)
{
	if (bus >= RCP_BUS_MAX || !is_registered(bus)) {
		return;
	}

	(void)rcp_os_event_set_bits(&bus_ctx.bus_evts[bus].evt, RCP_BUS_FLAG_TX_PENDING);
}

void rcp_bus_signal_rx_ready(enum rcp_bus bus)
{
	if (bus >= RCP_BUS_MAX || !is_registered(bus)) {
		return;
	}

	(void)rcp_os_event_set_bits(&bus_ctx.bus_evts[bus].evt, RCP_BUS_FLAG_RX_DATA);
}

void rcp_bus_clear_flags(enum rcp_bus bus, uint8_t clr_flags)
{
	if (bus >= RCP_BUS_MAX || !is_registered(bus) || clr_flags == 0) {
		return;
	}

	(void)rcp_os_event_clear_bits(&bus_ctx.bus_evts[bus].evt, clr_flags);
}

void __rcp_bus_set_flags(enum rcp_bus bus, uint8_t set_flags, uint8_t clr_flags)
{
	if (bus >= RCP_BUS_MAX || !is_registered(bus)) {
		return;
	}

	if (set_flags) {
		(void)rcp_os_event_set_bits(&bus_ctx.bus_evts[bus].evt, set_flags);
	}
	if (clr_flags) {
		(void)rcp_os_event_clear_bits(&bus_ctx.bus_evts[bus].evt, clr_flags);
	}
}

enum rcp_bus_status rcp_bus_get_flags(enum rcp_bus bus, uint8_t *flags)
{
	if (bus >= RCP_BUS_MAX || !is_registered(bus) || flags == NULL) {
		return RCP_BUS_ERR_INVALID;
	}

	*flags = get_bus_flags(bus);
	return RCP_BUS_OK;
}

uint16_t rcp_bus_get_rx_len(enum rcp_bus bus)
{
	if (!is_registered(bus)) {
		return RCP_BUS_ERR_INVALID;
	}
	return get_inst(bus)->get_rx_len();
}

bool rcp_bus_runtime_recover(enum rcp_bus bus)
{
	if (!is_registered(bus)) {
		return false;
	}

	return get_inst(bus)->recover();
}

const char *rcp_bus_status_to_str(enum rcp_bus_status status)
{
	size_t index;

	if (status > RCP_BUS_OK || status < RCP_BUS_ERR_UNKNOWN) {
		return "UNKNOWN";
	}

	index = (size_t)-status;
	if (index >= sizeof(status_string) / sizeof(status_string[0])) {
		return "UNKNOWN";
	}

	return status_string[index];
}

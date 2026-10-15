/*
 * Interworking - minimal stub for AP-only builds
 *
 * The full STA-side Interworking/ANQP client (interworking.c) pulls in a large
 * dependency chain (gas_query, roaming_consortium, credentials). For the
 * QuickTrack APDUT use case only the AP-side Interworking IE / ANQP server
 * (gas_serv.c) is needed; the STA-side auto-ANQP fetch is never exercised
 * (auto_interworking stays 0).
 *
 * CONFIG_INTERWORKING is defined globally (under CONFIG_WFA) so that the AP
 * advertises the Interworking IE / ANQP server. That also pulls in the
 * STA-side interworking control path in events.c and ctrl_iface.c, which
 * references these symbols. Since interworking.c is not compiled, the stubs
 * below satisfy those link references (all returning failure / no-op) without
 * pulling in the full client.
 *
 * This software may be distributed under the terms of the BSD license.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "wpa_supplicant_i.h"

#ifdef CONFIG_INTERWORKING

void interworking_start_fetch_anqp(struct wpa_supplicant *wpa_s)
{
	/* STA-side auto ANQP fetch not supported in AP-only build. */
}

int interworking_fetch_anqp(struct wpa_supplicant *wpa_s)
{
	/* STA-side ANQP fetch not supported in AP-only build. */
	return -1;
}

void interworking_stop_fetch_anqp(struct wpa_supplicant *wpa_s)
{
	/* No-op: nothing to stop in AP-only build. */
}

int interworking_select(struct wpa_supplicant *wpa_s, int auto_select,
			int *freqs)
{
	/* STA-side network selection via Interworking not supported. The real
	 * implementation takes ownership of freqs (stores it into
	 * wpa_s->manual_scan_freqs), so free it here to avoid leaking the
	 * caller-allocated array. */
	os_free(freqs);
	return -1;
}

int interworking_connect(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
			 int only_add)
{
	/* STA-side Interworking connection not supported. */
	return -1;
}

#ifdef CONFIG_HS20
/*
 * Only referenced from ctrl_iface.c under CONFIG_HS20 (not currently enabled).
 * Stubbed defensively so that enabling CONFIG_HS20 in the future does not
 * reintroduce an undefined reference here.
 */
int interworking_home_sp_cred(struct wpa_supplicant *wpa_s,
			      struct wpa_cred *cred,
			      struct wpabuf *domain_names)
{
	return -1;
}
#endif /* CONFIG_HS20 */

#endif /* CONFIG_INTERWORKING */

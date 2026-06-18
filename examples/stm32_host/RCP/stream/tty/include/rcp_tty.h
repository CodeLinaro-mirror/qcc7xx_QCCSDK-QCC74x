#ifndef RCP_TTY_H
#define RCP_TTY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int rcp_tty_init(void);
int rcp_tty_send(const uint8_t *data, uint16_t len, uint32_t timeout_ms);
int rcp_tty_read(uint8_t *buf, size_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* RCP_TTY_H */

#ifndef __RCP_UNIT_TEST_H__
#define __RCP_UNIT_TEST_H__

#include <stddef.h>

#include "rcp_mcu_capability.h"

typedef struct {
  enum mcu_cap_type group;
  const char *name;
  void (*handler)(int argc, char **argv);
  const char *help;
} shell_cmd_t;

extern const shell_cmd_t rcp_unit_test_commands[];
extern const size_t rcp_unit_test_commands_count;

#endif /* __RCP_UNIT_TEST_H__ */

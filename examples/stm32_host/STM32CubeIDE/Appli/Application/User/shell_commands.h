#ifndef __SHELL_COMMANDS_H__
#define __SHELL_COMMANDS_H__

#include <stdbool.h>

/* Initialize shell system */
bool shell_commands_init(void);

/* Start shell task */
void shell_task_start(void);

#endif /* __SHELL_COMMANDS_H__ */

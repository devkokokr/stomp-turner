#pragma once

// Task parameters for connect_switch_task, exposed so main.c can create it
// directly and keep a single place listing every task running in the system.
#define CONNECT_SWITCH_TASK_NAME       "connect_switch"
#define CONNECT_SWITCH_TASK_STACK_SIZE 3072
#define CONNECT_SWITCH_TASK_PRIORITY   5

// Configures the GP10 Connect switch GPIO. Call before creating
// connect_switch_task.
void connect_switch_init(void);

// Polls the Connect switch and turns a debounced short press into a manual
// advertising retry, and a debounced 3s+ long press into a bond-clear +
// fresh-pairing request. Run as a FreeRTOS task via CONNECT_SWITCH_TASK_*
// above.
void connect_switch_task(void *arg);

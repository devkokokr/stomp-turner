#pragma once

// Task parameters for input_task, exposed so main.c can create it directly
// and keep a single place listing every task running in the system.
#define INPUT_TASK_NAME        "footswitch_input"
#define INPUT_TASK_STACK_SIZE  3072
#define INPUT_TASK_PRIORITY    5

// Configures the footswitch and DIP-switch GPIOs and latches the
// key-mapping profile from the DIP switch. Call before creating input_task.
void input_init(void);

// Polls the footswitches and turns debounced transitions into BLE HID key
// presses, using the profile latched by input_init(). Run as a FreeRTOS
// task via INPUT_TASK_* above.
void input_task(void *arg);

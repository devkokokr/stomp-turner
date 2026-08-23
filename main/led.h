#pragma once

// Task parameters for led_task, exposed so main.c can create it directly and
// keep a single place listing every task running in the system.
#define LED_TASK_NAME       "status_led"
#define LED_TASK_STACK_SIZE 2560
#define LED_TASK_PRIORITY   2

// Configures the GP12 status LED GPIO. Call before creating led_task.
void led_init(void);

// Renders the GP12 status LED pattern (firmware.md "LED status display",
// plan A): a one-time boot self-check, then whichever pattern the current
// system state calls for, highest priority first -- low-voltage cutoff
// warning, low-battery warning, pairing mode, advertising/reconnecting, a
// one-shot "just connected" blink, or off while idle. Run as a FreeRTOS task
// via LED_TASK_* above.
void led_task(void *arg);

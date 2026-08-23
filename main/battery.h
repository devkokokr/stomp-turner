#pragma once

#include <stdint.h>

// Task parameters for battery_task, exposed so main.c can create it directly
// and keep a single place listing every task running in the system.
#define BATTERY_TASK_NAME       "battery_monitor"
#define BATTERY_TASK_STACK_SIZE 3072
#define BATTERY_TASK_PRIORITY   3

typedef enum {
    BATTERY_STATE_NORMAL,  // >= warning threshold
    BATTERY_STATE_WARNING, // below warning threshold, at or above cutoff
    BATTERY_STATE_CUTOFF,  // below cutoff threshold, shutdown in progress
} battery_state_t;

// Configures the GP11 ADC channel (with calibration, where the eFuse
// supports it) used for V_BAT sensing. Call before creating battery_task.
void battery_init(void);

// Periodically samples V_BAT (divider-corrected) and classifies it against
// the warning/cutoff thresholds. Once a cutoff reading holds for several
// consecutive cycles, battery_get_state() reports BATTERY_STATE_CUTOFF for a
// short grace period -- the window the status LED task uses for its
// fast-blink warning once that's implemented -- and then this task puts the
// device into deep sleep; only a power cycle brings it back. Run as a
// FreeRTOS task via BATTERY_TASK_* above.
void battery_task(void *arg);

// Most recent V_BAT reading in millivolts, already corrected for the
// on-board divider. 0 until the first sample completes.
uint32_t battery_get_voltage_mv(void);

// Most recent classified state, updated once per sampling cycle.
battery_state_t battery_get_state(void);

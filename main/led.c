#include "led.h"

#include <stdbool.h>
#include <stdint.h>

#include "battery.h"
#include "board.h"
#include "ble_hid.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Render tick. Fine enough to resolve the fastest pattern (cutoff, 10Hz/
// 50ms half-period) without waking the CPU much more often than input_task
// already does.
#define TICK_MS 20

#define BOOT_BLINK_MS 300

#define CUTOFF_HALF_PERIOD_MS 50   // 10Hz
#define PAIRING_HALF_PERIOD_MS 100 // 5Hz
#define ADV_HALF_PERIOD_MS    500  // 1Hz

#define WARNING_BLINK_MS    150
#define WARNING_BURST_COUNT 3
#define WARNING_PAUSE_MS    1500

#define CONNECTED_BLINK_MS 2000

#define PAIRING_SUCCESS_BLINK_MS   100
#define PAIRING_SUCCESS_BLINKS     2

static inline void set_led(bool on)
{
    gpio_set_level(BOARD_GPIO_STATUS_LED, on ? 1 : 0);
}

static inline int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

void led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_GPIO_STATUS_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    set_led(false);
}

// 3 short blinks then a longer pause, repeating -- low-battery warning.
static bool warning_burst_on(int64_t t)
{
    const int64_t blink_period_ms = 2 * WARNING_BLINK_MS;
    const int64_t burst_ms = WARNING_BURST_COUNT * blink_period_ms;
    const int64_t cycle_ms = burst_ms + WARNING_PAUSE_MS;

    int64_t phase = t % cycle_ms;
    if (phase >= burst_ms) {
        return false;
    }
    return (phase % blink_period_ms) < WARNING_BLINK_MS;
}

// Blocking: renders the "new pairing succeeded" double-blink cue inline,
// then returns. A few hundred ms of the LED task not evaluating the regular
// priority chain is harmless -- nothing else depends on this task's timing.
static void render_pairing_success_blink(void)
{
    for (int i = 0; i < PAIRING_SUCCESS_BLINKS; i++) {
        set_led(true);
        vTaskDelay(pdMS_TO_TICKS(PAIRING_SUCCESS_BLINK_MS));
        set_led(false);
        vTaskDelay(pdMS_TO_TICKS(PAIRING_SUCCESS_BLINK_MS));
    }
}

void led_task(void *arg)
{
    // Boot self-check.
    set_led(true);
    vTaskDelay(pdMS_TO_TICKS(BOOT_BLINK_MS));
    set_led(false);

    bool was_connected = false;
    int64_t connected_since_ms = 0;

    while (1) {
        if (ble_hid_consume_pairing_success()) {
            render_pairing_success_blink();
            was_connected = ble_hid_is_connected(); // resync after the blocking blink
            if (was_connected) {
                connected_since_ms = now_ms();
            }
        }

        bool connected = ble_hid_is_connected();
        if (connected && !was_connected) {
            connected_since_ms = now_ms();
        }
        was_connected = connected;

        int64_t t = now_ms();
        battery_state_t batt = battery_get_state();
        bool on;

        // Priority, highest first: cutoff warning > low-battery warning >
        // pairing mode > advertising/reconnecting > connected confirmation
        // blink (one-shot) > idle (off).
        if (batt == BATTERY_STATE_CUTOFF) {
            on = ((t / CUTOFF_HALF_PERIOD_MS) % 2) == 0;
        } else if (batt == BATTERY_STATE_WARNING) {
            on = warning_burst_on(t);
        } else if (ble_hid_is_pairing_mode()) {
            on = ((t / PAIRING_HALF_PERIOD_MS) % 2) == 0;
        } else if (!connected) {
            on = ((t / ADV_HALF_PERIOD_MS) % 2) == 0;
        } else {
            on = (t - connected_since_ms) < CONNECTED_BLINK_MS;
        }

        set_led(on);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

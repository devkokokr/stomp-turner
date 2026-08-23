#include "connect_switch.h"

#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "ble_hid.h"
#include "debounce.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "connect_switch";

#define POLL_INTERVAL_MS 10 // see input.c for the >=1 tick note this relies on
#define LONG_PRESS_MS     3000

static void configure_gpio(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << BOARD_GPIO_CONNECT_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

void connect_switch_init(void)
{
    configure_gpio();
}

void connect_switch_task(void *arg)
{
    debounced_input_t sw;
    debounce_init(&sw, BOARD_GPIO_CONNECT_BUTTON);

    bool pressed = false;
    bool long_press_fired = false;
    int64_t press_start_us = 0;

    while (1) {
        if (debounce_update(&sw)) {
            if (sw.stable_level == 0) {
                // Pulled up, switch shorts to GND when pressed.
                pressed = true;
                long_press_fired = false;
                press_start_us = esp_timer_get_time();
            } else if (pressed && !long_press_fired) {
                // Released before the long-press threshold.
                ESP_LOGI(TAG, "short press -> restart advertising");
                ble_hid_restart_advertising();
                pressed = false;
            } else {
                pressed = false;
            }
        }

        if (pressed && !long_press_fired &&
            (esp_timer_get_time() - press_start_us) >= (int64_t)LONG_PRESS_MS * 1000) {
            long_press_fired = true;
            ESP_LOGI(TAG, "long press -> clear bonds, enter pairing mode");
            ble_hid_enter_pairing_mode();
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

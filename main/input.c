#include "input.h"

#include <stdbool.h>
#include <stdint.h>

#include "board.h"
#include "ble_hid.h"
#include "debounce.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "input";

// Must resolve to >= 1 tick (CONFIG_FREERTOS_HZ=100 -> 10ms/tick) or
// vTaskDelay never actually blocks, starving the idle task and tripping the
// task watchdog.
#define POLL_INTERVAL_MS       10

typedef struct {
    uint8_t prev_key;
    uint8_t next_key;
} key_profile_t;

// Indexed by the 4-bit DIP value (GP9 GP8 GP7 GP6); see README "Key-mapping
// profiles" table. Entries not listed there are left zeroed and treated as
// unassigned, so they fall back to profile 0 in read_dip_profile().
static const key_profile_t s_profiles[16] = {
    [0] = { HID_KEY_LEFT_ARROW, HID_KEY_RIGHT_ARROW },
    [1] = { HID_KEY_UP_ARROW,   HID_KEY_DOWN_ARROW },
    [2] = { HID_KEY_PAGE_UP,    HID_KEY_PAGE_DOWN },
    [3] = { HID_KEY_BACKSPACE,  HID_KEY_SPACE },
    [4] = { HID_KEY_BACKSPACE,  HID_KEY_ENTER },
};

static uint8_t s_profile_index;

static void configure_input_gpio(gpio_num_t pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

// DIP pins idle high on the internal pull-up; a switch set to ON is assumed
// to short its pin to GND, so the raw level is inverted to get the bit value.
static uint8_t read_dip_profile(void)
{
    uint8_t bits = 0;
    bits |= (!gpio_get_level(BOARD_GPIO_DIP_BIT0)) << 0;
    bits |= (!gpio_get_level(BOARD_GPIO_DIP_BIT1)) << 1;
    bits |= (!gpio_get_level(BOARD_GPIO_DIP_BIT2)) << 2;
    bits |= (!gpio_get_level(BOARD_GPIO_DIP_BIT3)) << 3;

    if (s_profiles[bits].prev_key == 0 && s_profiles[bits].next_key == 0) {
        return 0; // unused combination -> default profile
    }
    return bits;
}

void input_task(void *arg)
{
    debounced_input_t left, right;
    debounce_init(&left, BOARD_GPIO_FOOTSWITCH_LEFT);
    debounce_init(&right, BOARD_GPIO_FOOTSWITCH_RIGHT);

    while (1) {
        // Latching switches have no "held" state to poll for repeat, so any
        // settled transition -- in either direction -- is one page-turn step.
        if (debounce_update(&left)) {
            ESP_LOGI(TAG, "left footswitch step -> previous");
            ble_hid_send_key(s_profiles[s_profile_index].prev_key);
        }
        if (debounce_update(&right)) {
            ESP_LOGI(TAG, "right footswitch step -> next");
            ble_hid_send_key(s_profiles[s_profile_index].next_key);
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

void input_init(void)
{
    configure_input_gpio(BOARD_GPIO_FOOTSWITCH_LEFT);
    configure_input_gpio(BOARD_GPIO_FOOTSWITCH_RIGHT);
    configure_input_gpio(BOARD_GPIO_DIP_BIT0);
    configure_input_gpio(BOARD_GPIO_DIP_BIT1);
    configure_input_gpio(BOARD_GPIO_DIP_BIT2);
    configure_input_gpio(BOARD_GPIO_DIP_BIT3);

    s_profile_index = read_dip_profile();
    ESP_LOGI(TAG, "key-mapping profile %u selected", s_profile_index);
}

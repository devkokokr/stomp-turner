#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "battery.h"
#include "ble_hid.h"
#include "connect_switch.h"
#include "input.h"
#include "led.h"
#include "power.h"

static const char *TAG = "stomp-turner";

void app_main(void)
{
    power_init();
    ble_hid_init();

    // Every task running in the system is created here so the full task
    // list is visible in one place.
    input_init();
    xTaskCreate(input_task, INPUT_TASK_NAME, INPUT_TASK_STACK_SIZE, NULL, INPUT_TASK_PRIORITY, NULL);

    battery_init();
    xTaskCreate(battery_task, BATTERY_TASK_NAME, BATTERY_TASK_STACK_SIZE, NULL, BATTERY_TASK_PRIORITY, NULL);

    connect_switch_init();
    xTaskCreate(connect_switch_task, CONNECT_SWITCH_TASK_NAME, CONNECT_SWITCH_TASK_STACK_SIZE, NULL,
                CONNECT_SWITCH_TASK_PRIORITY, NULL);

    led_init();
    xTaskCreate(led_task, LED_TASK_NAME, LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "stomp-turner boot OK");
}

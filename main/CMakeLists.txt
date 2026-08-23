idf_component_register(SRCS "battery.c" "ble_hid.c" "connect_switch.c" "debounce.c" "esp_hid_gap.c" "input.c" "led.c" "main.c" "power.c"
                        INCLUDE_DIRS "."
                        REQUIRES esp_hid bt esp_driver_gpio
                        PRIV_REQUIRES nvs_flash esp_adc esp_pm)

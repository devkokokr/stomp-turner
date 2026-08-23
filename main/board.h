#pragma once

#include "driver/gpio.h"

// Foot switches (latching/toggle type, not momentary — see firmware notes)
#define BOARD_GPIO_FOOTSWITCH_LEFT   GPIO_NUM_4  // "previous"
#define BOARD_GPIO_FOOTSWITCH_RIGHT  GPIO_NUM_5  // "next"

// DIP switch, 4 bits, forms the key-mapping profile index (GP9 = MSB, GP6 = LSB)
#define BOARD_GPIO_DIP_BIT0          GPIO_NUM_6
#define BOARD_GPIO_DIP_BIT1          GPIO_NUM_7
#define BOARD_GPIO_DIP_BIT2          GPIO_NUM_8
#define BOARD_GPIO_DIP_BIT3          GPIO_NUM_9

#define BOARD_GPIO_CONNECT_BUTTON    GPIO_NUM_10
#define BOARD_GPIO_BATTERY_ADC       GPIO_NUM_11
#define BOARD_GPIO_STATUS_LED        GPIO_NUM_12

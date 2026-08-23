#pragma once

#include <stdbool.h>
#include <stdint.h>

// USB HID keyboard usage IDs (Keyboard/Keypad page), subset used by this project
#define HID_KEY_ENTER       0x28
#define HID_KEY_BACKSPACE   0x2A
#define HID_KEY_SPACE       0x2C
#define HID_KEY_PAGE_UP     0x4B
#define HID_KEY_PAGE_DOWN   0x4E
#define HID_KEY_RIGHT_ARROW 0x4F
#define HID_KEY_LEFT_ARROW  0x50
#define HID_KEY_DOWN_ARROW  0x51
#define HID_KEY_UP_ARROW    0x52

// Brings up NimBLE, the HID-over-GATT service and starts advertising.
void ble_hid_init(void);

bool ble_hid_is_connected(void);

// Sends a single key press followed by a release.
void ble_hid_send_key(uint8_t keycode);

// Called by esp_hid_gap.c once the BLE link has been encrypted (bonding/
// pairing completed). Reports must not be sent before this point.
void ble_hid_task_start_up(void);

// Restarts advertising if it isn't already running. Called on a short press
// of the Connect switch (GP10) to manually retry a reconnect.
void ble_hid_restart_advertising(void);

// Disconnects any active link, wipes all stored bonds, and restarts
// advertising so a different host can pair fresh. Called on a long (3s+)
// press of the Connect switch (GP10). ble_hid_is_pairing_mode() reports true
// until the next successful pairing (or a timeout), for the status LED.
void ble_hid_enter_pairing_mode(void);

bool ble_hid_is_pairing_mode(void);

// True for one status-LED cycle right after a new pairing completes while in
// pairing mode; reading it clears the flag.
bool ble_hid_consume_pairing_success(void);

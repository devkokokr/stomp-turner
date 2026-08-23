/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef _ESP_HID_GAP_H_
#define _ESP_HID_GAP_H_

// Trimmed from Espressif's ble_hid_device_demo example down to the NimBLE
// path -- this project always builds with CONFIG_BT_NIMBLE_ENABLED=y (see
// sdkconfig.defaults) and never enables the Bluedroid Classic-BT/BLE stack,
// so that fallback path (GAP scanning, BT Classic pairing, etc.) doesn't
// apply here and was removed rather than carried around unused.
#define HIDD_BLE_MODE 0x01
#define HID_DEV_MODE  HIDD_BLE_MODE

#include "esp_err.h"
#include "esp_log.h"
#include "esp_bt.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_hid_gap_init(uint8_t mode);
esp_err_t esp_hid_gap_deinit(void);

esp_err_t esp_hid_ble_gap_adv_init(uint16_t appearance, const char *device_name);
esp_err_t esp_hid_ble_gap_adv_start(void);

// Disconnects the active link (if any) and wipes all stored bonds, then
// restarts advertising if it isn't already running -- so the next central to
// connect gets a fresh pairing instead of reusing (or being rejected due to)
// an old bond. Used to let a different host pair with this device.
void esp_hid_gap_clear_bonds_and_disconnect(void);

#ifdef __cplusplus
}
#endif

#endif /* _ESP_HID_GAP_H_ */

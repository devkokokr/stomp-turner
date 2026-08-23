#include "ble_hid.h"

#include <stdio.h>
#include <string.h>

#include "esp_hid_gap.h"
#include "esp_hidd.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "ble_hid";

// How long ble_hid_is_pairing_mode() stays true without a successful pairing
// before falling back to normal advertising -- so a forgotten pairing
// attempt doesn't leave the status LED fast-blinking (and radio advertising
// at the shorter pairing-mode interval) indefinitely.
#define PAIRING_MODE_TIMEOUT_MS 60000

// Standard boot-keyboard style report: 1 modifier byte, 1 reserved byte,
// 1-byte LED output report, and up to 5 simultaneous keycodes.
static const unsigned char s_keyboard_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0,        //   Usage Minimum (0xE0)
    0x29, 0xE7,        //   Usage Maximum (0xE7)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs) - modifier byte
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Const,Var,Abs) - reserved byte
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (Num Lock)
    0x29, 0x05,        //   Usage Maximum (Kana)
    0x91, 0x02,        //   Output (Data,Var,Abs) - LED report
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x03,        //   Output (Const,Var,Abs) - LED padding
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0x00,        //   Usage Minimum (0x00)
    0x29, 0x65,        //   Usage Maximum (0x65)
    0x81, 0x00,        //   Input (Data,Array,Abs) - up to 5 simultaneous keycodes
    0xC0,               // End Collection
};

#define KEYBOARD_REPORT_ID  1
// modifier (1) + reserved (1) + 5 keycodes = 7 bytes. The LED fields in the
// report map above are a separate Output report (host -> device), not part
// of this Input report, so they don't add to this length.
#define KEYBOARD_REPORT_LEN 7

// How long the key-down report is held before the key-up report follows.
#define KEY_RELEASE_DELAY_MS 20

static esp_hid_raw_report_map_t s_report_maps[] = {
    {
        .data = s_keyboard_report_map,
        .len = sizeof(s_keyboard_report_map),
    },
};

// Filled in at init time with the last 2 bytes of the BLE MAC address, so
// multiple units powered on at once show up as distinct devices when pairing.
static char s_device_name[32];

static esp_hid_device_config_t s_hid_config = {
    .vendor_id         = 0x16C0,
    .product_id        = 0x05DF,
    .version           = 0x0100,
    .device_name       = s_device_name,
    .manufacturer_name = "stomp-turner project",
    .serial_number     = "0001",
    .report_maps       = s_report_maps,
    .report_maps_len   = 1,
};

static void build_device_name(void)
{
    // esp_hid_gap.c packs flags + appearance + tx power + this name + a 16-bit
    // service UUID into a single legacy advertising PDU (31-byte hard limit).
    // The other fields take 14 bytes, so the name must stay <= 15 chars.
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(s_device_name, sizeof(s_device_name), "StompTurner%02X", mac[5]);
}

static esp_hidd_dev_t *s_hid_dev = NULL;
static volatile bool s_connected = false;

static volatile bool s_pairing_mode = false;
static volatile bool s_pairing_success_pending = false;
static int64_t s_pairing_mode_deadline_us = 0;

// Called by esp_hid_gap.c once the BLE link has been encrypted (bonding/pairing
// completed). Reports must not be sent before this point.
void ble_hid_task_start_up(void)
{
    ESP_LOGI(TAG, "link encrypted, ready to send reports");
    if (s_pairing_mode) {
        // Encryption only completes after a fresh pairing handshake while in
        // pairing mode (a plain reconnect of an already-bonded host doesn't
        // go through this exact path with s_pairing_mode still set), so this
        // is specifically "a new device just paired".
        s_pairing_mode = false;
        s_pairing_success_pending = true;
    }
    s_connected = true;
}

void ble_hid_restart_advertising(void)
{
    if (ble_gap_adv_active()) {
        ESP_LOGI(TAG, "already advertising, ignoring restart request");
        return;
    }
    esp_err_t ret = esp_hid_ble_gap_adv_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "failed to restart advertising: %s", esp_err_to_name(ret));
    }
}

void ble_hid_enter_pairing_mode(void)
{
    ESP_LOGI(TAG, "entering pairing mode: clearing bonds, restarting advertising");
    esp_hid_gap_clear_bonds_and_disconnect();
    s_pairing_mode = true;
    s_pairing_mode_deadline_us = esp_timer_get_time() + (int64_t)PAIRING_MODE_TIMEOUT_MS * 1000;
}

bool ble_hid_is_pairing_mode(void)
{
    if (s_pairing_mode && esp_timer_get_time() > s_pairing_mode_deadline_us) {
        // No new pairing within the timeout -- fall back to normal
        // advertising rather than fast-blinking (and radio-advertising at
        // the shorter pairing interval) forever.
        s_pairing_mode = false;
    }
    return s_pairing_mode;
}

bool ble_hid_consume_pairing_success(void)
{
    if (s_pairing_success_pending) {
        s_pairing_success_pending = false;
        return true;
    }
    return false;
}

static void hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    switch ((esp_hidd_event_t)id) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "started, advertising");
        esp_hid_ble_gap_adv_start();
        break;
    case ESP_HIDD_CONNECT_EVENT:
        // Link isn't encrypted yet at this point; s_connected is set once
        // ble_hid_task_start_up() confirms encryption, not here.
        ESP_LOGI(TAG, "connected, waiting for link encryption");
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "disconnected, restarting advertising");
        s_connected = false;
        esp_hid_ble_gap_adv_start();
        break;
    case ESP_HIDD_STOP_EVENT:
        ESP_LOGI(TAG, "stopped");
        break;
    default:
        break;
    }
}

static void nimble_host_task(void *param)
{
    // Returns only after nimble_port_stop() is called.
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// No public header ships this declaration -- ESP-IDF's own NimBLE example
// code (e.g. protocomm_nimble.c) forward-declares it the same way.
void ble_store_config_init(void);

void ble_hid_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    build_device_name();
    ESP_LOGI(TAG, "device name: %s", s_device_name);

    ESP_ERROR_CHECK(esp_hid_gap_init(HID_DEV_MODE));
    ESP_ERROR_CHECK(esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, s_hid_config.device_name));
    ESP_ERROR_CHECK(esp_hidd_dev_init(&s_hid_config, ESP_HID_TRANSPORT_BLE, hidd_event_callback, &s_hid_dev));

    // esp_hidd_dev_init() is what registers the GAP service; its Device Name
    // and Appearance characteristics otherwise stay at the NimBLE defaults
    // ("nimble" / Unknown) even though the advertised fields above are correct.
    if (ble_svc_gap_device_name_set(s_device_name) != 0) {
        ESP_LOGW(TAG, "ble_svc_gap_device_name_set failed");
    }
    if (ble_svc_gap_device_appearance_set(ESP_HID_APPEARANCE_KEYBOARD) != 0) {
        ESP_LOGW(TAG, "ble_svc_gap_device_appearance_set failed");
    }

    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    nimble_port_freertos_init(nimble_host_task);
}

bool ble_hid_is_connected(void)
{
    return s_connected;
}

void ble_hid_send_key(uint8_t keycode)
{
    if (!s_connected) {
        ESP_LOGW(TAG, "not connected, dropping key 0x%02x", keycode);
        return;
    }

    ESP_LOGI(TAG, "sending key 0x%02x", keycode);

    uint8_t report[KEYBOARD_REPORT_LEN] = {0};
    report[2] = keycode;
    esp_err_t ret = esp_hidd_dev_input_set(s_hid_dev, 0, KEYBOARD_REPORT_ID, report, KEYBOARD_REPORT_LEN);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "key-down report failed: %s", esp_err_to_name(ret));
    }

    vTaskDelay(pdMS_TO_TICKS(KEY_RELEASE_DELAY_MS));
    memset(report, 0, sizeof(report));
    ret = esp_hidd_dev_input_set(s_hid_dev, 0, KEYBOARD_REPORT_ID, report, KEYBOARD_REPORT_LEN);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "key-up report failed: %s", esp_err_to_name(ret));
    }
}

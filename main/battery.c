#include "battery.h"

#include <stdbool.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "battery";

// GP11 = ADC2 channel 0 on the ESP32-S3 (soc/adc_channel.h). ADC2 is shared
// with the Wi-Fi driver on this chip, but this project is BLE-only and never
// enables Wi-Fi, so that restriction doesn't apply here.
#define BATTERY_ADC_UNIT    ADC_UNIT_2
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_0
#define BATTERY_ADC_ATTEN   ADC_ATTEN_DB_12

// R8/R9 100K/100K divider halves V_BAT before it reaches the ADC pin
// (hardware.md: V_BAT = ADC reading x 2).
#define BATTERY_DIVIDER_RATIO 2

// hardware.md / firmware.md thresholds (confirmed).
#define BATTERY_WARNING_MV 3900
#define BATTERY_CUTOFF_MV  3800

#define SAMPLE_INTERVAL_MS    2000
#define SAMPLES_PER_READING   8 // averaged per cycle to smooth ADC noise
// Consecutive low cycles required before treating a cutoff reading as real
// rather than a transient sag under BLE TX load.
#define CUTOFF_CONFIRM_CYCLES 3
// How long BATTERY_STATE_CUTOFF is held (and reported via
// battery_get_state()) before deep sleep actually starts -- gives the status
// LED task room for its fast-blink warning once that's wired up.
#define CUTOFF_GRACE_MS 3000

static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_cali_handle;
static bool s_calibrated;

static volatile uint32_t s_voltage_mv;
static volatile battery_state_t s_state = BATTERY_STATE_NORMAL;

static bool init_calibration(void)
{
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration unavailable (%s), falling back to nominal scaling", esp_err_to_name(ret));
        return false;
    }
    return true;
}

void battery_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL, &chan_cfg));

    s_calibrated = init_calibration();
}

// Reads BATTERY_ADC_CHANNEL SAMPLES_PER_READING times and returns the
// divider-corrected V_BAT in millivolts. out_raw_avg/out_pin_mv are filled in
// with the pre-divider intermediate values for diagnostic logging.
static uint32_t sample_voltage_mv(int *out_raw_avg, int *out_pin_mv)
{
    int raw_sum = 0;
    for (int i = 0; i < SAMPLES_PER_READING; i++) {
        int raw;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw));
        raw_sum += raw;
    }
    int raw_avg = raw_sum / SAMPLES_PER_READING;

    int pin_mv;
    if (s_calibrated) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali_handle, raw_avg, &pin_mv));
    } else {
        // Uncalibrated fallback: scale the 12-bit raw reading across the
        // attenuation's nominal full-scale range (~3300mV for DB_12).
        pin_mv = (raw_avg * 3300) / 4095;
    }

    *out_raw_avg = raw_avg;
    *out_pin_mv = pin_mv;
    return (uint32_t)pin_mv * BATTERY_DIVIDER_RATIO;
}

static battery_state_t classify(uint32_t mv)
{
    if (mv < BATTERY_CUTOFF_MV) {
        return BATTERY_STATE_CUTOFF;
    }
    if (mv < BATTERY_WARNING_MV) {
        return BATTERY_STATE_WARNING;
    }
    return BATTERY_STATE_NORMAL;
}

void battery_task(void *arg)
{
    uint8_t cutoff_streak = 0;

    while (1) {
        int raw_avg, pin_mv;
        uint32_t mv = sample_voltage_mv(&raw_avg, &pin_mv);
        s_voltage_mv = mv;

        battery_state_t state = classify(mv);
        cutoff_streak = (state == BATTERY_STATE_CUTOFF) ? cutoff_streak + 1 : 0;

        ESP_LOGI(TAG, "V_BAT %lu mV (pin=%d mV, raw=%d/4095, calibrated=%d, state=%d)",
                 (unsigned long)mv, pin_mv, raw_avg, (int)s_calibrated, (int)state);
        s_state = state;

        if (cutoff_streak >= CUTOFF_CONFIRM_CYCLES) {
            ESP_LOGW(TAG, "V_BAT %lu mV sustained below cutoff (%d mV); shutting down in %d ms",
                     (unsigned long)mv, BATTERY_CUTOFF_MV, CUTOFF_GRACE_MS);
            vTaskDelay(pdMS_TO_TICKS(CUTOFF_GRACE_MS));
            // Only the power slide switch brings the device back; no wakeup
            // source is configured on purpose (confirmed behavior, firmware.md).
            esp_deep_sleep_start();
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}

uint32_t battery_get_voltage_mv(void)
{
    return s_voltage_mv;
}

battery_state_t battery_get_state(void)
{
    return s_state;
}

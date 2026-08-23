#include "power.h"

#include "esp_log.h"
#include "esp_pm.h"

static const char *TAG = "power";

void power_init(void)
{
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_XTAL_FREQ,
        .light_sleep_enable = true,
    };
    esp_err_t ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_pm_configure failed: %s (automatic light sleep disabled)", esp_err_to_name(ret));
    }
}

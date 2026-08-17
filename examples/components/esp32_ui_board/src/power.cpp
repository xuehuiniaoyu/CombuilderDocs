#include "board/power.h"
#include "board/pins.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

static const char * TAG = "BoardPower";

esp_err_t BoardPower::init() {
#if !USE_POWER
    return ESP_OK;
#else
    if (POWER_LOCK_PIN >= 0) {
        gpio_config_t io = {};
        io.pin_bit_mask = 1ULL << POWER_LOCK_PIN;
        io.mode = GPIO_MODE_OUTPUT;
        esp_err_t err = gpio_config(&io);
        if (err != ESP_OK) return err;
        gpio_set_level((gpio_num_t)POWER_LOCK_PIN, 1);
        lock_ok_ = true;
        ESP_LOGI(TAG, "POWER_LOCK GPIO%d = HIGH", POWER_LOCK_PIN);
    }
    if (POWER_CHARGING_PIN >= 0) {
        gpio_config_t io = {};
        io.pin_bit_mask = 1ULL << POWER_CHARGING_PIN;
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io);
    }
    return ESP_OK;
#endif
}

bool BoardPower::charging() const {
#if USE_POWER
    if (POWER_CHARGING_PIN < 0) return false;
    /* 星智：低电平通常表示充电中（依硬件；此处仅采样） */
    return gpio_get_level((gpio_num_t)POWER_CHARGING_PIN) == 0;
#else
    return false;
#endif
}

void BoardPower::shutdown() {
#if USE_POWER
    ESP_LOGW(TAG, "shutdown");
    if (POWER_LOCK_PIN >= 0) {
        gpio_set_level((gpio_num_t)POWER_LOCK_PIN, 0);
        gpio_hold_en((gpio_num_t)POWER_LOCK_PIN);
    }
    esp_deep_sleep_start();
#endif
}

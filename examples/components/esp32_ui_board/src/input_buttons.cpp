#include "board/input.h"
#include "board/board.h"
#include "board/pins.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static const char * TAG = "BoardInput";

void BoardInput::add(const char * name, int gpio, bool active_low, uint32_t long_ms, uint32_t repeat_ms) {
    if (count_ >= (int)(sizeof(btns_) / sizeof(btns_[0])) || gpio < 0) return;
    Btn & b = btns_[count_++];
    b = {};
    b.name = name;
    b.gpio = gpio;
    b.active_low = active_low;
    b.long_ms = long_ms;
    b.repeat_ms = repeat_ms ? repeat_ms : 120;

    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << gpio;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io.pull_down_en = active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    gpio_config(&io);
}

esp_err_t BoardInput::init() {
#if !USE_BUTTONS
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (ready_) return ESP_OK;
    count_ = 0;
    /* 导航键：约 0.45s 进入长按连发；电源键仍用较长阈值避免误触 */
    add("boot", BUTTON_BOOT_PIN, true, 450, 120);
    add("volume_up", BUTTON_VOLUME_UP_PIN, true, 450, 120);
    add("volume_down", BUTTON_VOLUME_DOWN_PIN, true, 450, 120);
    add("power", BUTTON_POWER_PIN, true, 3000, 120);
    ready_ = true;
    ESP_LOGI(TAG, "buttons registered: %d", count_);
    return ESP_OK;
#endif
}

void BoardInput::emit(Btn & b, BoardBtnEvent ev) {
    if (cb_) {
        cb_(b.name, ev);
        return;
    }
    handleDefault(b.name, ev);
}

/** 无 setCallback 时的兜底：仅硬件侧行为。UI 焦点/确认由 main 的 setCallback → Application 负责。 */
void BoardInput::handleDefault(const std::string & name, BoardBtnEvent ev) {
    if (name == "boot" && ev == BoardBtnEvent::DoubleClick) {
        ESP_LOGI(TAG, "boot double → back");
        if (hooks_.back) hooks_.back();
        return;
    }
    if (name == "power" && ev == BoardBtnEvent::Click) {
        ESP_LOGI(TAG, "power click → back");
        if (hooks_.back) hooks_.back();
        return;
    }
    if (name == "power" && ev == BoardBtnEvent::DoubleClick) {
        ESP_LOGI(TAG, "power double → home");
        if (hooks_.home) hooks_.home();
        return;
    }
    if ((ev == BoardBtnEvent::Click || ev == BoardBtnEvent::LongPress ||
         ev == BoardBtnEvent::LongPressRepeat) &&
        (name == "volume_up" || name == "volume_down")) {
        const int delta = (name == "volume_up") ? +10 : -10;
        if (hooks_.volume_delta) {
            hooks_.volume_delta(delta);
        } else {
            Board::instance().audio().setVolume(Board::instance().audio().volume() + delta);
        }
        ESP_LOGI(TAG, "volume=%d", Board::instance().audio().volume());
        return;
    }
    ESP_LOGD(TAG, "btn %s ev=%d", name.c_str(), (int)ev);
}

void BoardInput::loop() {
    if (!ready_) return;
    const int64_t now = esp_timer_get_time();
    for (int i = 0; i < count_; i++) {
        Btn & b = btns_[i];
        const int level = gpio_get_level((gpio_num_t)b.gpio);
        const bool down = b.active_low ? (level == 0) : (level != 0);

        if (down != b.stable) {
            static int64_t last_change[8] = {};
            if (now - last_change[i] < 20000) continue;
            last_change[i] = now;
            b.stable = down;

            if (down) {
                b.pressed = true;
                b.long_fired = false;
                b.down_us = now;
                b.last_repeat_us = 0;
            } else if (b.pressed) {
                b.pressed = false;
                if (b.long_fired) {
                    /* 已长按/连发：松手不再生成单击 */
                    b.long_fired = false;
                    b.click_count = 0;
                } else {
                    if (b.click_count > 0 && (now - b.last_click_us) < 400000) {
                        b.click_count++;
                    } else {
                        b.click_count = 1;
                    }
                    b.last_click_us = now;
                    if (b.click_count >= 2) {
                        emit(b, BoardBtnEvent::DoubleClick);
                        b.click_count = 0;
                    }
                }
            }
        }

        /* 按住：到阈值发 LongPress，之后周期性 LongPressRepeat */
        if (b.pressed && b.stable) {
            const int64_t held_ms = (now - b.down_us) / 1000;
            if (!b.long_fired && held_ms >= (int64_t)b.long_ms) {
                b.long_fired = true;
                b.last_repeat_us = now;
                emit(b, BoardBtnEvent::LongPress);
            } else if (b.long_fired &&
                       (now - b.last_repeat_us) >= (int64_t)b.repeat_ms * 1000) {
                b.last_repeat_us = now;
                emit(b, BoardBtnEvent::LongPressRepeat);
            }
        }

        if (!b.pressed && b.click_count == 1 && (now - b.last_click_us) > 350000) {
            emit(b, BoardBtnEvent::Click);
            b.click_count = 0;
        }
    }
}

#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus

class BoardDisplay {
public:
    esp_err_t init();
    bool ready() const { return ready_; }

    int width() const { return width_; }
    int height() const { return height_; }

    /** 0–100；PWM 背光，失败时回退 GPIO */
    void setBacklight(int percent);
    int backlight() const { return bl_percent_; }

    /** 整屏写黑（跳页前清掉 LCD 残留，绕过 LVGL 局部缓冲） */
    void clearBlack();

    esp_lcd_panel_handle_t panel() const { return panel_; }
    esp_lcd_panel_io_handle_t panelIo() const { return panel_io_; }

private:
    bool ready_ = false;
    int width_ = 0;
    int height_ = 0;
    int bl_percent_ = 100;
    int bl_pin_ = -1;
    bool use_ledc_ = false;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    esp_err_t initBacklightGpioOff();
    esp_err_t enableBacklight(int percent);
};

#endif

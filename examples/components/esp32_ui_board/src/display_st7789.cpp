#include "board/display.h"
#include "board/pins.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <lvgl.h>
#include <algorithm>

static const char * TAG = "BoardDisplay";

esp_err_t BoardDisplay::initBacklightGpioOff() {
    if (bl_pin_ < 0) return ESP_OK;
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << bl_pin_;
    io.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "bl gpio");
    const int off = DISPLAY_BACKLIGHT_OUTPUT_INVERT ? 1 : 0;
    gpio_set_level((gpio_num_t)bl_pin_, off);
    return ESP_OK;
}

esp_err_t BoardDisplay::enableBacklight(int percent) {
    if (bl_pin_ < 0) return ESP_OK;
    percent = std::clamp(percent, 0, 100);
    bl_percent_ = percent;

    /* LEDC PWM 25kHz / 10bit — 与 comlua/星智一致 */
    if (!use_ledc_) {
        ledc_timer_config_t t = {};
        t.speed_mode = LEDC_LOW_SPEED_MODE;
        t.duty_resolution = LEDC_TIMER_10_BIT;
        t.timer_num = LEDC_TIMER_0;
        t.freq_hz = 25000;
        t.clk_cfg = LEDC_AUTO_CLK;
        if (ledc_timer_config(&t) == ESP_OK) {
            ledc_channel_config_t ch = {};
            ch.gpio_num = bl_pin_;
            ch.speed_mode = LEDC_LOW_SPEED_MODE;
            ch.channel = LEDC_CHANNEL_0;
            ch.timer_sel = LEDC_TIMER_0;
            ch.duty = 0;
            ch.hpoint = 0;
            ch.flags.output_invert = DISPLAY_BACKLIGHT_OUTPUT_INVERT ? 1 : 0;
            if (ledc_channel_config(&ch) == ESP_OK) {
                use_ledc_ = true;
                ESP_LOGI(TAG, "backlight LEDC PWM on GPIO%d", bl_pin_);
            }
        }
    }

    if (use_ledc_) {
        uint32_t duty = (uint32_t)percent * 1023 / 100;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        return ESP_OK;
    }

    const int on = DISPLAY_BACKLIGHT_OUTPUT_INVERT ? 0 : 1;
    const int off = DISPLAY_BACKLIGHT_OUTPUT_INVERT ? 1 : 0;
    gpio_set_level((gpio_num_t)bl_pin_, percent > 0 ? on : off);
    return ESP_OK;
}

void BoardDisplay::setBacklight(int percent) { (void)enableBacklight(percent); }

void BoardDisplay::clearBlack() {
#if !USE_DISPLAY
    return;
#else
    if (!panel_ || width_ <= 0 || height_ <= 0) return;
    /* 按条带刷黑（勿逐行）：逐行 SPI 过慢；勿在 LVGL 运行后与 flush 抢总线 */
    const int strip_h = 40;
    const size_t px = (size_t)width_ * (size_t)strip_h;
    uint16_t * buf = (uint16_t *)heap_caps_malloc(px * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) buf = (uint16_t *)malloc(px * sizeof(uint16_t));
    if (!buf) return;
    for (size_t i = 0; i < px; i++) buf[i] = 0x0000;
    for (int y = 0; y < height_; y += strip_h) {
        const int h = (y + strip_h <= height_) ? strip_h : (height_ - y);
        (void)esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + h, buf);
    }
    free(buf);
#endif
}

esp_err_t BoardDisplay::init() {
#if !USE_DISPLAY
    ESP_LOGW(TAG, "USE_DISPLAY=0, skip");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (ready_) return ESP_OK;

    width_ = DISPLAY_WIDTH;
    height_ = DISPLAY_HEIGHT;
    bl_pin_ = DISPLAY_SPI_BL_PIN;

    ESP_LOGI(TAG, "ST7789 %dx%d SPI host=%d", width_, height_, (int)DISPLAY_SPI_HOST);

    spi_bus_config_t bus = {};
    bus.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
    bus.miso_io_num = GPIO_NUM_NC;
    bus.sclk_io_num = DISPLAY_SPI_SCLK_PIN;
    bus.quadwp_io_num = GPIO_NUM_NC;
    bus.quadhd_io_num = GPIO_NUM_NC;
    bus.max_transfer_sz = width_ * height_ * (int)sizeof(uint16_t);

    ESP_RETURN_ON_ERROR(spi_bus_initialize(DISPLAY_SPI_HOST, &bus, DISPLAY_SPI_DMA_CH), TAG, "spi_bus");

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num = DISPLAY_SPI_CS_PIN;
    io_cfg.dc_gpio_num = DISPLAY_SPI_DC_PIN;
    io_cfg.spi_mode = DISPLAY_SPI_MODE;
    io_cfg.pclk_hz = DISPLAY_SPI_PCLK_HZ;
    io_cfg.trans_queue_depth = DISPLAY_TRANS_QUEUE_DEPTH;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_cfg, &panel_io_), TAG, "panel_io");

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = DISPLAY_SPI_RST_PIN;
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_cfg.bits_per_pixel = 16;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(panel_io_, &panel_cfg, &panel_), TAG, "st7789");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel_), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_), TAG, "init");
    esp_lcd_panel_invert_color(panel_, DISPLAY_COLOR_INVERT);
    esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

    (void)initBacklightGpioOff();
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_, true), TAG, "disp_on");
    /* 烧录/复位后面板上常残留上一固件画面；局部 LVGL 缓冲不会整屏覆盖，先直写清黑 */
    clearBlack();

    /* LVGL port */
    lv_init();
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 12;
    port_cfg.task_stack = 8192;
    port_cfg.timer_period_ms = 16;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl_port");

    const size_t buf_px = (size_t)width_ * 40;
    const size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const bool use_spiram = free_spiram > buf_px * 2 * sizeof(uint16_t);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = buf_px,
        .double_buffer = true,
        .trans_size = 0,
        .hres = (uint32_t)width_,
        .vres = (uint32_t)height_,
        .monochrome = false,
        .rotation =
            {
                .swap_xy = DISPLAY_SWAP_XY,
                .mirror_x = DISPLAY_MIRROR_X,
                .mirror_y = DISPLAY_MIRROR_Y,
            },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags =
            {
                .buff_dma = 1,
                .buff_spiram = use_spiram ? 1U : 0U,
                .sw_rotate = 0,
                .swap_bytes = 1,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };
    lv_display_t * disp = lvgl_port_add_disp(&disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return ESP_FAIL;
    }

    /* 先清黑；背光延后到 UI 首帧就绪（main 里 board_set_backlight），避免白闪/空屏闪 */
    if (lvgl_port_lock(100)) {
        lv_obj_t * scr = lv_display_get_screen_active(disp);
        if (scr) {
            lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
            lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        }
        lv_refr_now(disp);
        lvgl_port_unlock();
    }
    /* 保持背光关闭（initBacklightGpioOff 已拉低） */

    ready_ = true;
    ESP_LOGI(TAG, "display ready (spiram_buf=%d, backlight deferred)", (int)use_spiram);
    return ESP_OK;
#endif
}

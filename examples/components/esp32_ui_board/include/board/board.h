/**
 * ESP32 UI — 统一 Board 入口
 *
 * 用法（固件 main）：
 *   board_init();          // 电源 → 显示/LVGL → 按键 → 音频
 *   app_boot();            // UI 应用
 *   board_bind_application(app);
 *   for (;;) { board_loop(); ... }
 *
 * 板型由 menuconfig「ESP32 UI Board」选择；引脚见 board/pins.h。
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#include "board/display.h"
#include "board/input.h"
#include "board/audio.h"
#include "board/power.h"

class Board {
public:
    static Board & instance();

    /** 一次性初始化全部硬件（可重复调用，已初始化则跳过） */
    esp_err_t init();
    /** 轮询按键等（非阻塞）；LVGL 刷新由 esp_lvgl_port 任务负责 */
    void loop();

    const char * name() const;
    BoardDisplay & display();
    BoardInput & input();
    BoardAudio & audio();
    BoardPower & power();

    bool ready() const { return ready_; }

    /** 绑定 Application*（可选，供上层缓存） */
    void bindApplication(void * app);
    void * boundApplication() const { return app_; }

    /** 按键导航钩子（固件 main 绑定 goBack / 回首页） */
    void setNavHooks(const BoardNavHooks & hooks);

private:
    Board() = default;
    bool ready_ = false;
    void * app_ = nullptr;
};

extern "C" {
#endif

esp_err_t board_init(void);
void board_loop(void);
const char * board_name(void);
bool board_ready(void);
void board_bind_application(void * app);
void * board_bound_application(void);

typedef struct {
    void (*back)(void);
    void (*home)(void);
    void (*volume_delta)(int delta);
} board_nav_hooks_t;
void board_set_nav_hooks(const board_nav_hooks_t * hooks);

int board_display_width(void);
int board_display_height(void);
void board_set_backlight(int percent);
/** 仅 display init（LVGL 启动前）整屏清黑；运行期勿调，会与 LVGL SPI 冲突 */
void board_display_clear_black(void);
/**
 * 已废弃：勿再锁内直写面板。保留符号以免旧固件链接失败；始终返回 false。
 */
bool board_display_begin_page_flip(void);
void board_display_end_page_flip(void);

/** 实现 ui_platform_*（application_boot / export 约定） */
void ui_platform_init(void);
void ui_platform_loop(void);

#ifdef __cplusplus
}
#endif

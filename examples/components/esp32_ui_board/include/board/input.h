#pragma once

#include "esp_err.h"
#include <functional>
#include <string>

#ifdef __cplusplus

enum class BoardBtnEvent {
    Click,
    DoubleClick,
    LongPress,
    /** 长按阈值后、仍按住时周期性触发 */
    LongPressRepeat,
};

using BoardBtnCallback = std::function<void(const std::string & name, BoardBtnEvent ev)>;

/** C 钩子：固件 main 绑定导航，避免 board 依赖 app_project */
struct BoardNavHooks {
    void (*back)(void) = nullptr;
    void (*home)(void) = nullptr;
    void (*volume_delta)(int delta) = nullptr;
};

class BoardInput {
public:
    esp_err_t init();
    void loop();

    void setCallback(BoardBtnCallback cb) { cb_ = std::move(cb); }
    void setNavHooks(const BoardNavHooks & h) { hooks_ = h; }

private:
    struct Btn {
        const char * name;
        int gpio;
        bool active_low;
        uint32_t long_ms;
        uint32_t repeat_ms;
        bool pressed = false;
        bool stable = false;
        bool long_fired = false;
        int64_t down_us = 0;
        int64_t last_repeat_us = 0;
        int64_t last_click_us = 0;
        int click_count = 0;
    };

    bool ready_ = false;
    BoardBtnCallback cb_;
    BoardNavHooks hooks_{};
    Btn btns_[8]{};
    int count_ = 0;

    void add(const char * name, int gpio, bool active_low, uint32_t long_ms, uint32_t repeat_ms = 120);
    void emit(Btn & b, BoardBtnEvent ev);
    void handleDefault(const std::string & name, BoardBtnEvent ev);
};

#endif

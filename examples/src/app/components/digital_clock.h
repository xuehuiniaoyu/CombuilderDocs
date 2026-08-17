#pragma once
#include "lvgl.h"

/**
 * 数字时钟组件（自定义 Component）
 * JSON：ui/components/digital_clock.json
 * Activity 内嵌 type=component 后，用 attachLabels() 绑定再 setTime()。
 */
class DigitalClock {
public:
    void build(lv_obj_t * parent);

    void attachLabels(lv_obj_t * time_label, lv_obj_t * date_label);
    void setTime(const char * hhmmss, const char * yyyymmdd);

private:
    lv_obj_t * time_label_ = nullptr;
    lv_obj_t * date_label_ = nullptr;
};

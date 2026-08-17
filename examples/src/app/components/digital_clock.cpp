#include "digital_clock.h"
#include "digital_clock_ui.h"
#include <cstring>

void DigitalClock::build(lv_obj_t * parent) {
    ui_comp_DigitalClock_build(parent);
    attachLabels(ui_comp_DigitalClock_find("clock_time"), ui_comp_DigitalClock_find("clock_date"));
}

void DigitalClock::attachLabels(lv_obj_t * time_label, lv_obj_t * date_label) {
    time_label_ = time_label;
    date_label_ = date_label;
}

void DigitalClock::setTime(const char * hhmmss, const char * yyyymmdd) {
    /* 仅在文本变化时写 label，减少无意义 invalidate */
    if (time_label_ && hhmmss) {
        const char * cur = lv_label_get_text(time_label_);
        if (!cur || std::strcmp(cur, hhmmss) != 0) lv_label_set_text(time_label_, hhmmss);
    }
    if (date_label_ && yyyymmdd) {
        const char * cur = lv_label_get_text(date_label_);
        if (!cur || std::strcmp(cur, yyyymmdd) != 0) lv_label_set_text(date_label_, yyyymmdd);
    }
}

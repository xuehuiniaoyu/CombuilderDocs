#include "main_activity.h"
#include "main_activity_ui.h"
#include "application_base.h"
#include <cstdio>
#include <ctime>

namespace {

lv_obj_t * find_clock_view(MainActivity * self, const char * id) {
    if (!self || !id) return nullptr;
    if (lv_obj_t * o = self->findView(id)) return o;
    return ui_MainActivity_find(id);
}

} // namespace

MainActivity::MainActivity() = default;

MainActivity::~MainActivity() {
    stopClock();
}

void MainActivity::buildUi(lv_obj_t * screen) {
    ui_MainActivity_build(screen);
}

void MainActivity::onCreate() {
    std::printf("[Clock] MainActivity::onCreate\n");
    alive_.store(true);
    last_time_[0] = 0;
    last_date_[0] = 0;
    ntp_watching_ = false;

    clock_.attachLabels(find_clock_view(this, "clock_time"), find_clock_view(this, "clock_date"));

    /* 短按 Boot → 列表；长按 Boot → WiFi 配网（参考 comlua） */
    bindKey("boot", []() { navigateTo("ListActivity"); });
    bindKey("boot", "long_press", []() { navigateTo("WiFiConfigActivity"); });

    /*
     * NTP 观察者：已同步 → 立刻刷新；未同步 → 登记，成功后广播再刷。
     * App::onCreate 会 syncNtp()；此处只关心「时间可用了」这一刻。
     */
    if (Application * app = getApplication()) {
        ntp_watching_ = true;
        const bool ready = app->whenNtpSynced(&MainActivity::onNtpObserver, this);
        if (ready) ntp_watching_ = false;
    }

    startClock();
}

void MainActivity::onResume() {
    /* 配网页返回后：重新登记 NTP 观察者，收到广播则刷新时钟 */
    if (Application * app = getApplication()) {
        if (ntp_watching_) {
            app->removeNtpObserver(&MainActivity::onNtpObserver, this);
            ntp_watching_ = false;
        }
        ntp_watching_ = true;
        const bool ready = app->whenNtpSynced(&MainActivity::onNtpObserver, this);
        if (ready) {
            ntp_watching_ = false;
            onNtpReady();
        }
    }
}

void MainActivity::onDestroy() {
    if (ntp_watching_) {
        if (Application * app = getApplication()) {
            app->removeNtpObserver(&MainActivity::onNtpObserver, this);
        }
        ntp_watching_ = false;
    }
    stopClock();
    alive_.store(false);
    std::printf("[Clock] MainActivity::onDestroy\n");
}

void MainActivity::startClock() {
    stopClock();
    ui_timer_ = lv_timer_create(&MainActivity::onUiTimer, 1000, this);
    tickClock();
}

void MainActivity::stopClock() {
    if (ui_timer_) {
        lv_timer_delete(ui_timer_);
        ui_timer_ = nullptr;
    }
}

void MainActivity::onUiTimer(lv_timer_t * t) {
    auto * self = static_cast<MainActivity *>(lv_timer_get_user_data(t));
    if (self && self->alive_.load()) self->tickClock();
}

void MainActivity::onNtpObserver(void * user) {
    auto * self = static_cast<MainActivity *>(user);
    if (!self || !self->alive_.load()) return;
    self->ntp_watching_ = false;
    self->onNtpReady();
}

void MainActivity::onNtpReady() {
    std::printf("[Clock] NTP ready → refresh\n");
    last_time_[0] = 0;
    last_date_[0] = 0;
    tickClock();
}

void MainActivity::tickClock() {
    if (!alive_.load()) return;

    time_t now = time(nullptr);
    struct tm tm_now {};
#if defined(ESP_PLATFORM)
    localtime_r(&now, &tm_now);
#else
    tm_now = *localtime(&now);
#endif

    char time_buf[16]{};
    char date_buf[32]{};
    std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_now);
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &tm_now);

    if (std::strcmp(time_buf, last_time_) == 0 && std::strcmp(date_buf, last_date_) == 0) {
        return;
    }
    std::strncpy(last_time_, time_buf, sizeof(last_time_) - 1);
    std::strncpy(last_date_, date_buf, sizeof(last_date_) - 1);
    clock_.setTime(time_buf, date_buf);
}

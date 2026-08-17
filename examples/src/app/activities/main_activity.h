#pragma once
#include "activity_base.h"
#include "digital_clock.h"
#include "lvgl.h"
#include <atomic>
#include <cstring>

/** 时钟示例：UI 线程读表；NTP 用观察者，同步后强制刷一次 */
class MainActivity : public Activity {
public:
    MainActivity();
    ~MainActivity() override;

    void onCreate() override;
    void onResume() override;
    void onDestroy() override;
    void buildUi(lv_obj_t * screen) override;

private:
    void startClock();
    void stopClock();
    void tickClock();
    void onNtpReady();

    static void onUiTimer(lv_timer_t * t);
    static void onNtpObserver(void * user);

    DigitalClock clock_;
    lv_timer_t * ui_timer_ = nullptr;
    std::atomic<bool> alive_{false};
    bool ntp_watching_ = false;
    char last_time_[16]{};
    char last_date_[32]{};
};

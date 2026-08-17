#pragma once
#include "activity_base.h"
#include <atomic>

/** SoftAP 配网页（参考 comlua WiFiConfigActivity） */
class WiFiConfigActivity : public Activity {
public:
    void buildUi(lv_obj_t * screen) override;
    void onCreate() override;
    void onDestroy() override;

private:
    static void onSuccessUi(void * user);
    static void onNtpAfterProvision(void * user);

    std::atomic<bool> alive_{false};
    bool ntp_armed_ = false;
};

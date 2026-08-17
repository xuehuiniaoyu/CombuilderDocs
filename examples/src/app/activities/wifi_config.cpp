#include "wifi_config.h"
#include "wifi_config_ui.h"
#include "wifi_config_portal.h"
#include "application_base.h"
#include "navigate.h"
#include "alert.h"
#include "lvgl.h"
#include <cstdio>

void WiFiConfigActivity::buildUi(lv_obj_t * screen) {
    ui_WiFiConfigActivity_build(screen);
}

void WiFiConfigActivity::onNtpAfterProvision(void * user) {
    auto * self = static_cast<WiFiConfigActivity *>(user);
    if (!self || !self->alive_.load()) return;
    self->ntp_armed_ = false;
    alert("时间已同步", 2000);
    navigateBack(); /* 回主页；MainActivity 也会收到 NTP 广播刷新时钟 */
}

void WiFiConfigActivity::onSuccessUi(void * user) {
    auto * self = static_cast<WiFiConfigActivity *>(user);
    if (!self || !self->alive_.load()) return;

    alert("配网成功，正在同步时间…", 2500);

    /* 先停 SoftAP/HTTP，再强制 NTP（读 NVS 凭据）并广播观察者 */
    WiFiConfigPortal::instance().stop();

    Application * app = self->getApplication();
    if (!app) {
        navigateBack();
        return;
    }

    self->ntp_armed_ = true;
    app->resyncNtp();
    const bool already = app->whenNtpSynced(&WiFiConfigActivity::onNtpAfterProvision, self);
    if (already) {
        self->ntp_armed_ = false;
        alert("时间已同步", 2000);
        navigateBack();
    }
}

void WiFiConfigActivity::onCreate() {
    alive_.store(true);

    if (lv_obj_t * ins = findView("instruction")) {
        lv_obj_set_style_text_line_space(ins, -4, LV_PART_MAIN);
    }

    bindKey("boot", []() { navigateBack(); });

    alert("正在启动热点…", 1500);

    auto & portal = WiFiConfigPortal::instance();
    const bool ok = portal.start([this]() {
        lv_async_call(&WiFiConfigActivity::onSuccessUi, this);
    });

    /* 用真实 SoftAP 名刷新说明文案 */
    if (lv_obj_t * ins = findView("instruction")) {
        char text[320];
        std::snprintf(
            text,
            sizeof(text),
            "请按以下步骤进行配网：\n"
            "1. 手机连接设备WiFi\n"
            "   SSID: %s\n"
            "2. 浏览器打开：192.168.4.1\n"
            "3. 输入WiFi信息并连接\n"
            "4. 成功后自动同步时间并返回",
            portal.apSsid());
        lv_label_set_text(ins, text);
    }

#if defined(ESP_PLATFORM)
    if (ok) {
        char tip[72];
        std::snprintf(tip, sizeof(tip), "等待配网，请连接 %s", portal.apSsid());
        alert(tip, 2500);
    } else if (portal.isRunning()) {
        alert("HTTP失败", 3000);
    } else {
        alert("错误: WiFi AP启动失败", 3000);
    }
#else
    (void)ok;
    alert("WiFi已启动", 2500);
#endif

    std::printf("[WiFiConfig] portal start ok=%d ssid=%s\n", ok ? 1 : 0, portal.apSsid());
}

void WiFiConfigActivity::onDestroy() {
    if (ntp_armed_) {
        if (Application * app = getApplication()) {
            app->removeNtpObserver(&WiFiConfigActivity::onNtpAfterProvision, this);
        }
        ntp_armed_ = false;
    }
    alive_.store(false);
    WiFiConfigPortal::instance().stop();
}

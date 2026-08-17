#pragma once
#include <functional>
#include <string>

/**
 * SoftAP + HTTP 配网门户（对齐 comlua）
 * - AP SSID: ESP32-{id}，id 为设备 MAC 后 2 字节（4 位十六进制）
 * - 网页: http://192.168.4.1/
 * - POST /config 保存到 NVS(wifi_config)；成功后由 Activity 停门户并同步 NTP
 */
class WiFiConfigPortal {
public:
    static WiFiConfigPortal & instance();

    /** 启 SoftAP + HTTP(+DNS)。successCb 在配网成功后于后台任务调用（可 lv_async_call 改 UI） */
    bool start(std::function<void()> successCb = {});
    void stop();
    bool isRunning() const;

    /** 当前 SoftAP 名，形如 ESP32-A1B2；未生成前为 ESP32-0000 */
    const char * apSsid() const;

    static constexpr const char * kApIp = "192.168.4.1";

private:
    WiFiConfigPortal() = default;
    void refreshApSsid();
    bool startAp();
    bool startHttp();
    void startDns();
    void stopDns();

    void * http_ = nullptr; /* httpd_handle_t */
    void * dns_task_ = nullptr;
    std::function<void()> on_success_;
    bool ap_up_ = false;
    char ap_ssid_[16] = "ESP32-0000";
};

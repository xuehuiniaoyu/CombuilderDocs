#pragma once

#include "esp_err.h"
#include <cstddef>
#include <cstdint>

#ifdef __cplusplus

/**
 * 板级音频抽象。
 * 星智板：无 codec，I2S 直驱喇叭/麦（Simplex）。
 */
class BoardAudio {
public:
    esp_err_t init();
    bool available() const { return available_; }

    void setVolume(int percent); /* 0–100，软件衰减占位 */
    int volume() const { return volume_; }

    esp_err_t enableOutput(bool on);
    esp_err_t enableInput(bool on);

    /** 写出 PCM s16 mono/stereo（按板级配置） */
    esp_err_t write(const int16_t * samples, size_t count);
    esp_err_t read(int16_t * samples, size_t count);

private:
    bool available_ = false;
    bool out_on_ = false;
    bool in_on_ = false;
    int volume_ = 70;
    void * tx_handle_ = nullptr;
    void * rx_handle_ = nullptr;
};

#endif

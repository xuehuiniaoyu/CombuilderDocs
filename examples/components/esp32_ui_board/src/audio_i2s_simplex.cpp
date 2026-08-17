#include "board/audio.h"
#include "board/pins.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include <algorithm>
#include <cstring>

static const char * TAG = "BoardAudio";

esp_err_t BoardAudio::init() {
#if !USE_I2S_AUDIO
    available_ = false;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (available_) return ESP_OK;

    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_chan_handle_t tx = nullptr;
    esp_err_t err = i2s_new_channel(&tx_chan_cfg, &tx, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s tx channel: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t tx_std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = (gpio_num_t)I2S_SPK_BCLK_PIN,
                .ws = (gpio_num_t)I2S_SPK_WS_PIN,
                .dout = (gpio_num_t)I2S_SPK_DOUT_PIN,
                .din = I2S_GPIO_UNUSED,
                .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
            },
    };
    err = i2s_channel_init_std_mode(tx, &tx_std);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s tx std: %s", esp_err_to_name(err));
        return err;
    }
    tx_handle_ = tx;

    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    i2s_chan_handle_t rx = nullptr;
    err = i2s_new_channel(&rx_chan_cfg, nullptr, &rx);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2s rx channel: %s (speaker-only)", esp_err_to_name(err));
    } else {
        i2s_std_config_t rx_std = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
            .gpio_cfg =
                {
                    .mclk = I2S_GPIO_UNUSED,
                    .bclk = (gpio_num_t)I2S_MIC_BCLK_PIN,
                    .ws = (gpio_num_t)I2S_MIC_WS_PIN,
                    .dout = I2S_GPIO_UNUSED,
                    .din = (gpio_num_t)I2S_MIC_DIN_PIN,
                    .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
                },
        };
        err = i2s_channel_init_std_mode(rx, &rx_std);
        if (err == ESP_OK) {
            rx_handle_ = rx;
        } else {
            ESP_LOGW(TAG, "i2s rx std: %s", esp_err_to_name(err));
        }
    }

    available_ = true;
    ESP_LOGI(TAG, "I2S simplex ready rate=%d", I2S_SAMPLE_RATE);
    return ESP_OK;
#endif
}

void BoardAudio::setVolume(int percent) {
    volume_ = std::clamp(percent, 0, 100);
}

esp_err_t BoardAudio::enableOutput(bool on) {
#if USE_I2S_AUDIO
    if (!tx_handle_) return ESP_ERR_INVALID_STATE;
    auto * tx = static_cast<i2s_chan_handle_t>(tx_handle_);
    esp_err_t err = on ? i2s_channel_enable(tx) : i2s_channel_disable(tx);
    if (err == ESP_OK) out_on_ = on;
    return err;
#else
    (void)on;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t BoardAudio::enableInput(bool on) {
#if USE_I2S_AUDIO
    if (!rx_handle_) return ESP_ERR_INVALID_STATE;
    auto * rx = static_cast<i2s_chan_handle_t>(rx_handle_);
    esp_err_t err = on ? i2s_channel_enable(rx) : i2s_channel_disable(rx);
    if (err == ESP_OK) in_on_ = on;
    return err;
#else
    (void)on;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t BoardAudio::write(const int16_t * samples, size_t count) {
#if USE_I2S_AUDIO
    if (!tx_handle_ || !samples || !count) return ESP_ERR_INVALID_ARG;
    if (!out_on_) {
        ESP_RETURN_ON_ERROR(enableOutput(true), TAG, "enable out");
    }
    /* 简易软件音量 */
    size_t written = 0;
    if (volume_ >= 100) {
        return i2s_channel_write(static_cast<i2s_chan_handle_t>(tx_handle_), samples,
                                 count * sizeof(int16_t), &written, portMAX_DELAY);
    }
    /* 小缓冲衰减，避免改调用方数据 */
    int16_t tmp[256];
    size_t off = 0;
    while (off < count) {
        size_t n = std::min(count - off, sizeof(tmp) / sizeof(tmp[0]));
        for (size_t i = 0; i < n; i++) {
            tmp[i] = (int16_t)((int32_t)samples[off + i] * volume_ / 100);
        }
        size_t w = 0;
        esp_err_t err = i2s_channel_write(static_cast<i2s_chan_handle_t>(tx_handle_), tmp,
                                          n * sizeof(int16_t), &w, portMAX_DELAY);
        if (err != ESP_OK) return err;
        off += n;
    }
    return ESP_OK;
#else
    (void)samples;
    (void)count;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t BoardAudio::read(int16_t * samples, size_t count) {
#if USE_I2S_AUDIO
    if (!rx_handle_ || !samples || !count) return ESP_ERR_INVALID_ARG;
    if (!in_on_) {
        ESP_RETURN_ON_ERROR(enableInput(true), TAG, "enable in");
    }
    size_t read_bytes = 0;
    return i2s_channel_read(static_cast<i2s_chan_handle_t>(rx_handle_), samples,
                            count * sizeof(int16_t), &read_bytes, portMAX_DELAY);
#else
    (void)samples;
    (void)count;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

#include "board/board.h"
#include "esp_log.h"

static const char * TAG = "ui_platform";

extern "C" void ui_platform_init(void) {
    ESP_LOGI(TAG, "ui_platform_init → board_init");
    esp_err_t err = board_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "board_init failed: %s", esp_err_to_name(err));
    }
}

extern "C" void ui_platform_loop(void) {
    board_loop();
}

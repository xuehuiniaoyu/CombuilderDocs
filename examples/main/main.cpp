/**
 * 固件入口：NVS → Board → UI boot → 按键轮询
 */
#include "board/board.h"
#include "board/input.h"
#include "app_registry.h"
#include "application_base.h"
#include "storage_fs.h"
#include "ui_assets_mmap.h"
#include "ui_font.h"
#include "debug_serial.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_lvgl_port.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <string>

static const char * TAG = "main";
static Application * g_app = nullptr;

static void nav_back(void) {
    if (!g_app) return;
    if (lvgl_port_lock(100)) {
        g_app->goBack();
        lvgl_port_unlock();
    }
}

static void nav_home(void) {
    if (!g_app) return;
    if (lvgl_port_lock(100)) {
        while (g_app->goBack()) {
        }
        lvgl_port_unlock();
    }
}

static void nav_volume(int delta) {
    auto & audio = Board::instance().audio();
    audio.setVolume(audio.volume() + delta);
    ESP_LOGI(TAG, "volume=%d", audio.volume());
}

/** 板级按键 → Application（焦点/确认/返回）；默认 handleDefault 只会调音量 */
static void on_board_button(const std::string & name, BoardBtnEvent ev) {
    if (!g_app) return;

    const char * action = "click";
    if (ev == BoardBtnEvent::DoubleClick) action = "double_click";
    else if (ev == BoardBtnEvent::LongPress) action = "long_press";
    else if (ev == BoardBtnEvent::LongPressRepeat) action = "long_press_repeat";

    ESP_LOGI(TAG, "key %s %s → UI", name.c_str(), action);

    if (lvgl_port_lock(150)) {
        g_app->dispatchButton(name, action);
        const bool powered_off = g_app->isPoweredOff();
        const bool restart = g_app->consumeRestartRequest();
        lvgl_port_unlock();

        board_set_backlight(powered_off ? 0 : 100);
        if (restart) {
            ESP_LOGI(TAG, "power → restart");
            esp_restart();
        }
    }
    /* 未注册 long_press 时由 Application 把长按/连发回退为 click（如音量键连移焦点） */
}

static void boot_ui(void) {
    storage_init();
    if (ui_assets_mmap_init() != 0) {
        ESP_LOGW(TAG, "assets mmap unavailable — fonts/images may use LittleFS");
    }
    ui_fonts_init();
    ui_fonts_apply_to_active_screen();
    app_registry_init();

    g_app = app_create_application();
    if (!g_app) {
        ESP_LOGE(TAG, "create Application failed");
        return;
    }
    g_app->setActivityCreator([](const std::string & name) -> Activity * {
        return app_create_activity(name.c_str());
    });
    board_bind_application(g_app);
    /* app.json services[]（IoService 等）；勿漏，否则 getAs 一直为空 */
    app_start_services(g_app);
    g_app->onCreate();

    Activity * act = app_create_activity("MainActivity");
    if (!act) {
        ESP_LOGE(TAG, "MainActivity missing — 请先在扩展里「编译工程」");
        board_set_backlight(100);
        return;
    }
    if (lvgl_port_lock(500)) {
        g_app->startActivity(act);
        /* 再刷一帧，确保首屏像素已提交后再开背光 */
        lv_refr_now(nullptr);
        lvgl_port_unlock();
    }
    board_set_backlight(100);

    board_nav_hooks_t hooks = {
        .back = nav_back,
        .home = nav_home,
        .volume_delta = nav_volume,
    };
    board_set_nav_hooks(&hooks);
    Board::instance().input().setCallback(on_board_button);
    debug_serial_set_activity_json_provider(+[](char * buf, size_t buflen) -> size_t {
        if (!buf || buflen < 4 || !g_app) return 0;
        const std::string js = g_app->debugStackJson();
        if (js.size() + 1 > buflen) return 0;
        memcpy(buf, js.c_str(), js.size() + 1);
        return js.size();
    });
    debug_serial_start();
    ESP_LOGI(TAG, "UI ready, app=%s", g_app->getName().c_str());
}

static void boot_ui_task(void * arg) {
    (void)arg;
    boot_ui();
    vTaskDelete(nullptr);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Combuilder firmware starting");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(board_init());
    ESP_LOGI(TAG, "board=%s %dx%d", board_name(), board_display_width(), board_display_height());

    /* 字库/资源加载可能较重，放到大栈任务，避免主任务 OOM/栈溢出重启 */
    const uint32_t stack = 24 * 1024;
    if (xTaskCreate(boot_ui_task, "boot_ui", stack, nullptr, 5, nullptr) != pdPASS) {
        ESP_LOGW(TAG, "boot_ui task create failed, fallback inline");
        boot_ui();
    }

    for (;;) {
        board_loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

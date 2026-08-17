#include "board/board.h"
#include "board/pins.h"
#include "esp_log.h"

static const char * TAG = "Board";

static BoardDisplay g_display;
static BoardInput g_input;
static BoardAudio g_audio;
static BoardPower g_power;

Board & Board::instance() {
    static Board b;
    return b;
}

const char * Board::name() const { return BOARD_NAME; }
BoardDisplay & Board::display() { return g_display; }
BoardInput & Board::input() { return g_input; }
BoardAudio & Board::audio() { return g_audio; }
BoardPower & Board::power() { return g_power; }

void Board::bindApplication(void * app) { app_ = app; }

void Board::setNavHooks(const BoardNavHooks & hooks) {
    g_input.setNavHooks(hooks);
}

esp_err_t Board::init() {
    if (ready_) return ESP_OK;
    ESP_LOGI(TAG, "init board=%s", BOARD_NAME);

    esp_err_t err = g_power.init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "power init: %s", esp_err_to_name(err));
    }

#if USE_DISPLAY
    err = g_display.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display init failed: %s", esp_err_to_name(err));
        return err;
    }
#endif

#if USE_BUTTONS
    err = g_input.init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "input init: %s", esp_err_to_name(err));
    }
#endif

#if USE_I2S_AUDIO
    err = g_audio.init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "audio init: %s", esp_err_to_name(err));
    }
#endif

    ready_ = true;
    ESP_LOGI(TAG, "board ready (%dx%d)", g_display.width(), g_display.height());
    return ESP_OK;
}

void Board::loop() {
#if USE_BUTTONS
    g_input.loop();
#endif
}

extern "C" esp_err_t board_init(void) { return Board::instance().init(); }
extern "C" void board_loop(void) { Board::instance().loop(); }
extern "C" const char * board_name(void) { return Board::instance().name(); }
extern "C" bool board_ready(void) { return Board::instance().ready(); }
extern "C" void board_bind_application(void * app) { Board::instance().bindApplication(app); }
extern "C" void * board_bound_application(void) { return Board::instance().boundApplication(); }

extern "C" void board_set_nav_hooks(const board_nav_hooks_t * hooks) {
    BoardNavHooks h;
    if (hooks) {
        h.back = hooks->back;
        h.home = hooks->home;
        h.volume_delta = hooks->volume_delta;
    }
    Board::instance().setNavHooks(h);
}

extern "C" int board_display_width(void) {
#if USE_DISPLAY
    return Board::instance().display().width();
#else
    return DISPLAY_WIDTH;
#endif
}
extern "C" int board_display_height(void) {
#if USE_DISPLAY
    return Board::instance().display().height();
#else
    return DISPLAY_HEIGHT;
#endif
}
extern "C" void board_set_backlight(int percent) {
#if USE_DISPLAY
    Board::instance().display().setBacklight(percent);
#else
    (void)percent;
#endif
}

extern "C" void board_display_clear_black(void) {
#if USE_DISPLAY
    /* 仅应在 LVGL port 尚未向面板 flush 时调用（如 display init）；运行期清屏走 LVGL */
    Board::instance().display().clearBlack();
#endif
}

extern "C" bool board_display_begin_page_flip(void) {
    /* 保留 API：不再在锁内直写面板（会与 esp_lvgl_port SPI 死锁/喂狗失败） */
    return false;
}

extern "C" void board_display_end_page_flip(void) {
}

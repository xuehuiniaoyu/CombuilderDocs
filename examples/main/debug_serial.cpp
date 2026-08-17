/**
 * 真机调试：串口命令 → 拉取当前屏幕 LVGL 快照（RGB565 → Base64 行）
 *
 * 主机发送: @@ESP32UI_SNAP\n
 * 设备经 USB Serial/JTAG（或 UART stdin）接收。
 *
 * 注意：usb_serial_jtag_read_bytes 仅在 driver_install 成功后可调，
 * 否则 p_usb_serial_jtag_obj==NULL → LoadProhibited。
 */
#include "debug_serial.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "mbedtls/base64.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if __has_include("driver/usb_serial_jtag.h")
#include "driver/usb_serial_jtag.h"
#define DEBUG_HAS_USJ 1
#else
#define DEBUG_HAS_USJ 0
#endif

static const char * TAG = "debug_serial";
static const char CMD_SNAP[] = "@@ESP32UI_SNAP";
static const char CMD_MEM[] = "@@ESP32UI_MEM";
#if DEBUG_HAS_USJ
static bool g_usj_driver = false;
#endif
static debug_activity_json_fn g_activity_json_fn = nullptr;

void debug_serial_set_activity_json_provider(debug_activity_json_fn fn) {
  g_activity_json_fn = fn;
}

static void debug_write(const void * data, size_t n) {
  if (!data || n == 0) return;
  /* 禁止 stdout + USJ 双写：主控台已是 USB Serial/JTAG 时会把每行发两遍，
   * 主机拼出的 base64 全是噪点。优先走已 install 的驱动，否则只用 stdout。 */
#if DEBUG_HAS_USJ
  if (g_usj_driver && usb_serial_jtag_is_driver_installed()) {
    (void)usb_serial_jtag_write_bytes(data, n, pdMS_TO_TICKS(200));
    return;
  }
#endif
  fwrite(data, 1, n, stdout);
  fflush(stdout);
}

static void debug_printf(const char * fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n > 0) debug_write(buf, (size_t)((n < (int)sizeof(buf)) ? n : (int)sizeof(buf) - 1));
}

static int debug_read_byte(uint8_t * out) {
  if (!out) return 0;
#if DEBUG_HAS_USJ
  if (g_usj_driver && usb_serial_jtag_is_driver_installed()) {
    if (usb_serial_jtag_read_bytes(out, 1, 0) == 1) return 1;
  }
#endif
  int fd = fileno(stdin);
  if (fd >= 0) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    int r = (int)read(fd, out, 1);
    if (r == 1) return 1;
  }
  return 0;
}

/** 紧凑打包 RGB565 行后分块 Base64，避免 stride 填充干扰 */
static void b64_send_rgb565_packed(const uint8_t * data, uint32_t w, uint32_t h, uint32_t stride) {
  const size_t row_bytes = (size_t)w * 2u;
  const size_t raw_chunk = 120; /* 须为偶数，对齐像素 */
  unsigned char out[200];
  uint8_t chunk[120];
  size_t chunk_fill = 0;
  size_t seq = 0;

  auto flush_chunk = [&]() {
    if (chunk_fill == 0) return;
    size_t olen = 0;
    if (mbedtls_base64_encode(out, sizeof(out) - 1, &olen, chunk, chunk_fill) != 0) {
      debug_printf("@@SNAP_END err=b64\n");
      chunk_fill = 0;
      return;
    }
    out[olen] = 0;
    debug_printf("@@SNAP_B64 %s\n", (char *)out);
    chunk_fill = 0;
    if ((++seq % 8) == 0) vTaskDelay(pdMS_TO_TICKS(5));
  };

  for (uint32_t y = 0; y < h; y++) {
    const uint8_t * row = data + (size_t)y * stride;
    size_t left = row_bytes;
    size_t off = 0;
    while (left > 0) {
      size_t n = raw_chunk - chunk_fill;
      if (n > left) n = left;
      memcpy(chunk + chunk_fill, row + off, n);
      chunk_fill += n;
      off += n;
      left -= n;
      if (chunk_fill >= raw_chunk) flush_chunk();
    }
  }
  flush_chunk();
}

static void handle_snap(void) {
  ESP_LOGI(TAG, "snapshot request");
#if !LV_USE_SNAPSHOT
  debug_printf("@@SNAP_END err=no_snapshot_api\n");
  return;
#else
  if (!lvgl_port_lock(2000)) {
    debug_printf("@@SNAP_END err=lvgl_lock\n");
    return;
  }

  lv_obj_t * scr = lv_screen_active();
  if (!scr) {
    lvgl_port_unlock();
    debug_printf("@@SNAP_END err=no_screen\n");
    return;
  }

  /* 强制刷新一帧，避免快照到空/旧层 */
  lv_obj_invalidate(scr);
  lv_refr_now(lv_display_get_default());

  lv_draw_buf_t * buf = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB565);
  lvgl_port_unlock();

  if (!buf || !buf->data) {
    if (buf) lv_draw_buf_destroy(buf);
    debug_printf("@@SNAP_END err=snapshot_failed\n");
    return;
  }

  const uint32_t w = buf->header.w;
  const uint32_t h = buf->header.h;
  const uint32_t stride = buf->header.stride;
  const uint32_t packed = w * h * 2u;
  if (w == 0 || h == 0 || stride < w * 2u || packed == 0 || stride * h > buf->data_size) {
    lv_draw_buf_destroy(buf);
    debug_printf("@@SNAP_END err=bad_size\n");
    return;
  }

  /* endian=le：ESP32 原生；stride 按紧凑行上报，主机按 w*2 解 */
  debug_printf("@@SNAP_BEGIN w=%u h=%u fmt=rgb565 endian=le bytes=%u stride=%u\n",
               (unsigned)w, (unsigned)h, (unsigned)packed, (unsigned)(w * 2u));
  b64_send_rgb565_packed(buf->data, w, h, stride);
  debug_printf("@@SNAP_END ok\n");
  lv_draw_buf_destroy(buf);
  ESP_LOGI(TAG, "snapshot sent %ux%u packed=%u", (unsigned)w, (unsigned)h, (unsigned)packed);
#endif
}

static void handle_mem(void) {
  lv_mem_monitor_t mon;
  memset(&mon, 0, sizeof(mon));
  char activity[768];
  activity[0] = '{';
  activity[1] = '}';
  activity[2] = 0;

  if (lvgl_port_lock(500)) {
    lv_mem_monitor(&mon);
    if (g_activity_json_fn) {
      size_t n = g_activity_json_fn(activity, sizeof(activity));
      if (n == 0 || n >= sizeof(activity)) {
        activity[0] = '{';
        activity[1] = '}';
        activity[2] = 0;
      }
    }
    lvgl_port_unlock();
  }

  const size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t internal_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t internal_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const size_t spiram_largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const size_t total_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);

  /* 大缓冲：含 Activity JSON，不能走 256B debug_printf */
  char line[1536];
  int n = snprintf(
      line, sizeof(line),
      "@@MEM_JSON {"
      "\"ok\":true,"
      "\"source\":\"device\","
      "\"lvgl\":{"
      "\"total_size\":%u,"
      "\"free_size\":%u,"
      "\"used_pct\":%u,"
      "\"frag_pct\":%u,"
      "\"max_used\":%u,"
      "\"free_biggest\":%u"
      "},"
      "\"esp\":{"
      "\"internal_free\":%u,"
      "\"internal_min\":%u,"
      "\"internal_largest\":%u,"
      "\"spiram_free\":%u,"
      "\"spiram_largest\":%u,"
      "\"total_free\":%u"
      "},"
      "\"widgets\":0,"
      "\"activity\":%s"
      "}\n",
      (unsigned)mon.total_size,
      (unsigned)mon.free_size,
      (unsigned)mon.used_pct,
      (unsigned)mon.frag_pct,
      (unsigned)mon.max_used,
      (unsigned)mon.free_biggest_size,
      (unsigned)internal_free,
      (unsigned)internal_min,
      (unsigned)internal_largest,
      (unsigned)spiram_free,
      (unsigned)spiram_largest,
      (unsigned)total_free,
      activity);
  if (n > 0) {
    size_t len = (size_t)n;
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    debug_write(line, len);
  }
}

static void debug_serial_task(void * arg) {
  (void)arg;
  char line[96];
  size_t pos = 0;
  ESP_LOGI(TAG, "debug serial task ready (send %s / %s) usj=%d", CMD_SNAP, CMD_MEM,
           (int)g_usj_driver);

  for (;;) {
    uint8_t ch = 0;
    if (!debug_read_byte(&ch)) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (ch == '\r') continue;
    if (ch == '\n') {
      line[pos] = 0;
      if (pos > 0) {
        if (strstr(line, CMD_MEM)) handle_mem();
        else if (strstr(line, CMD_SNAP)) handle_snap();
      }
      pos = 0;
      continue;
    }
    if (pos + 1 < sizeof(line)) {
      line[pos++] = (char)ch;
    } else {
      pos = 0;
    }
  }
}

void debug_serial_start(void) {
  static bool started = false;
  if (started) return;
  started = true;

#if DEBUG_HAS_USJ
  /* 控制台若已是 USJ VFS，stdin 可读；额外 install 便于直接排空 USB RX，避免主机写满后断口复位 */
  if (usb_serial_jtag_is_driver_installed()) {
    g_usj_driver = true;
  } else {
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.rx_buffer_size = 1024;
    cfg.tx_buffer_size = 1024;
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err == ESP_OK) {
      g_usj_driver = true;
      ESP_LOGI(TAG, "USB Serial/JTAG driver installed for debug RX");
    } else {
      ESP_LOGW(TAG, "USJ driver install skipped (%s); rely on stdin", esp_err_to_name(err));
      g_usj_driver = false;
    }
  }
#endif

  if (xTaskCreate(debug_serial_task, "dbg_serial", 8192, nullptr, 4, nullptr) != pdPASS) {
    ESP_LOGW(TAG, "failed to start debug serial task");
    started = false;
  }
}

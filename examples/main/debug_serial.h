#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 启动真机调试串口任务（@@ESP32UI_SNAP / @@ESP32UI_MEM） */
void debug_serial_start(void);

/**
 * 可选：填充 Activity 栈 JSON（不含外层花括号以外的包装）。
 * 返回写入字节数（不含 NUL）；失败返回 0。
 */
typedef size_t (*debug_activity_json_fn)(char * buf, size_t buflen);

void debug_serial_set_activity_json_provider(debug_activity_json_fn fn);

#ifdef __cplusplus
}
#endif

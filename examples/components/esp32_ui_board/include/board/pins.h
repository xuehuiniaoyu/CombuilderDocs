#pragma once
/**
 * 按 menuconfig 选择的板型转发 pins.h
 * CMake 已把 boards/ 加入 include path
 */
#include "sdkconfig.h"

#if defined(CONFIG_ESP32_UI_BOARD_XINGZHI_CUBE_1_54TFT_ML307)
#include "xingzhi_cube_1_54tft_ml307/pins.h"
#elif defined(CONFIG_ESP32_UI_BOARD_NONE)
#include "none/pins.h"
#else
#include "xingzhi_cube_1_54tft_ml307/pins.h"
#endif

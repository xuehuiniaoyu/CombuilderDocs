#pragma once
/** 无板型占位：保证接口可编译，不初始化真实硬件 */
#include "driver/spi_common.h"

#define BOARD_NAME "none"

#define USE_DISPLAY              0
#define DISPLAY_PANEL_ST7789     0
#define DISPLAY_SPI_MOSI_PIN     -1
#define DISPLAY_SPI_SCLK_PIN     -1
#define DISPLAY_SPI_CS_PIN       -1
#define DISPLAY_SPI_DC_PIN       -1
#define DISPLAY_SPI_RST_PIN      -1
#define DISPLAY_SPI_BL_PIN       -1
#define DISPLAY_WIDTH            240
#define DISPLAY_HEIGHT           240
#define DISPLAY_OFFSET_X         0
#define DISPLAY_OFFSET_Y         0
#define DISPLAY_SWAP_XY          false
#define DISPLAY_MIRROR_X         false
#define DISPLAY_MIRROR_Y         false
#define DISPLAY_COLOR_INVERT     false
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE         0
#define DISPLAY_SPI_PCLK_HZ      (40 * 1000 * 1000)
#define DISPLAY_SPI_HOST         SPI2_HOST
#define DISPLAY_SPI_DMA_CH       SPI_DMA_CH_AUTO
#define DISPLAY_TRANS_QUEUE_DEPTH 10

#define USE_BUTTONS              0
#define BUTTON_BOOT_PIN          -1
#define BUTTON_VOLUME_UP_PIN     -1
#define BUTTON_VOLUME_DOWN_PIN   -1
#define BUTTON_POWER_PIN         -1

#define USE_POWER                0
#define POWER_LOCK_PIN           -1
#define POWER_CHARGING_PIN       -1
#define POWER_KEY_PIN            -1

#define USE_I2S_AUDIO            0
#define I2S_SAMPLE_RATE          16000
#define I2S_SPK_BCLK_PIN         -1
#define I2S_SPK_WS_PIN           -1
#define I2S_SPK_DOUT_PIN         -1
#define I2S_MIC_BCLK_PIN         -1
#define I2S_MIC_WS_PIN           -1
#define I2S_MIC_DIN_PIN          -1

#define USE_SD                   0

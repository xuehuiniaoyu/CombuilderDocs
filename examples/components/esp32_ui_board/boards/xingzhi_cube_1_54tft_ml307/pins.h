#pragma once
/**
 * 无名科技 星智 Cube 1.54TFT (ML307)
 * 参考：comlua / xiaozhi xingzhi-cube-1.54tft-ml307
 *
 * SoC: ESP32-S3
 * LCD: ST7789 SPI 240×240（无触摸）
 * Audio: I2S 直驱（无 ES8311）
 */
#include <driver/gpio.h>
#include "driver/spi_common.h"

#define BOARD_NAME "xingzhi-cube-1.54tft-ml307"

/* ---------- Display ST7789 ---------- */
#define USE_DISPLAY              1
#define DISPLAY_PANEL_ST7789     1
#define DISPLAY_SPI_MOSI_PIN     10
#define DISPLAY_SPI_SCLK_PIN     9
#define DISPLAY_SPI_CS_PIN       14
#define DISPLAY_SPI_DC_PIN       8
#define DISPLAY_SPI_RST_PIN      18
#define DISPLAY_SPI_BL_PIN       13
#define DISPLAY_WIDTH            240
#define DISPLAY_HEIGHT           240
#define DISPLAY_OFFSET_X         0
#define DISPLAY_OFFSET_Y         0
#define DISPLAY_SWAP_XY          false
#define DISPLAY_MIRROR_X         false
#define DISPLAY_MIRROR_Y         false
#define DISPLAY_COLOR_INVERT     true
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE         3
#define DISPLAY_SPI_PCLK_HZ      (40 * 1000 * 1000)
/* 星智：LCD 用 SPI2，与 I2S 分离，避免队列超时 */
#define DISPLAY_SPI_HOST         SPI2_HOST
#define DISPLAY_SPI_DMA_CH       SPI_DMA_CH_AUTO
#define DISPLAY_TRANS_QUEUE_DEPTH 20

/* ---------- Buttons ---------- */
#define USE_BUTTONS              1
#define BUTTON_BOOT_PIN          0
#define BUTTON_VOLUME_UP_PIN     40
#define BUTTON_VOLUME_DOWN_PIN   39
#define BUTTON_POWER_PIN         41

/* ---------- Power ---------- */
#define USE_POWER                1
#define POWER_LOCK_PIN           21
#define POWER_CHARGING_PIN       38
#define POWER_KEY_PIN            41

/* ---------- I2S Audio (simplex) ---------- */
#define USE_I2S_AUDIO            1
#define I2S_SAMPLE_RATE          16000
#define I2S_SPK_BCLK_PIN         15
#define I2S_SPK_WS_PIN           16
#define I2S_SPK_DOUT_PIN         7
#define I2S_MIC_BCLK_PIN         5
#define I2S_MIC_WS_PIN           4
#define I2S_MIC_DIN_PIN          6

/* ---------- SD（沿用 comlua 占位；官方星智未必接 SD） ---------- */
#define USE_SD                   0
#define SD_SDMMC_CLK_PIN         43
#define SD_SDMMC_CMD_PIN         42
#define SD_SDMMC_D0_PIN          44

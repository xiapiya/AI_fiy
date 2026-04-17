/**
 * @file tft_display.h
 * @brief ILI9341 TFT显示屏驱动 - 基于ESP-IDF原生LCD API
 *
 * 硬件配置:
 * - 屏幕型号: ILI9341 2.2寸 SPI模块
 * - 分辨率: 240x320
 * - 接口: SPI (40MHz)
 */

#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"

// ==================== TFT屏幕参数 ====================
#define TFT_H_RES        240
#define TFT_V_RES        320
#define TFT_BIT_PER_PIXEL 16

// ==================== SPI引脚定义 ====================
#define TFT_PIN_SCK      GPIO_NUM_13
#define TFT_PIN_MOSI     GPIO_NUM_11
#define TFT_PIN_CS       GPIO_NUM_10
#define TFT_PIN_DC       GPIO_NUM_12
#define TFT_PIN_RST      GPIO_NUM_3
#define TFT_PIN_BL       GPIO_NUM_5

// ==================== SPI配置 ====================
#define TFT_SPI_HOST     SPI2_HOST
#define TFT_SPI_FREQ_HZ  (40 * 1000 * 1000)  // 40MHz

/**
 * @brief 初始化TFT显示屏
 *
 * 配置SPI总线、初始化ILI9341面板、启用背光
 *
 * @param[out] panel_handle LCD面板句柄（用于后续操作）
 * @return ESP_OK 成功
 *         ESP_FAIL 失败
 */
esp_err_t tft_display_init(esp_lcd_panel_handle_t *panel_handle);

/**
 * @brief 设置背光亮度
 *
 * @param[in] brightness 亮度百分比 (0-100)
 * @return ESP_OK 成功
 */
esp_err_t tft_display_set_backlight(uint8_t brightness);

#endif // TFT_DISPLAY_H

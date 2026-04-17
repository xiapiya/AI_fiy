/**
 * @file tft_display.c
 * @brief ILI9341 TFT显示屏驱动实现
 *
 * 参考ESP-IDF官方示例: examples/peripherals/lcd/spi_lcd_touch
 */

#include "tft_display.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/ledc.h"

static const char *TAG = "TFT";

// 背光PWM配置
#define TFT_BL_PWM_TIMER   LEDC_TIMER_0
#define TFT_BL_PWM_MODE    LEDC_LOW_SPEED_MODE
#define TFT_BL_PWM_CHANNEL LEDC_CHANNEL_0
#define TFT_BL_PWM_FREQ_HZ 5000
#define TFT_BL_PWM_DUTY_RES LEDC_TIMER_10_BIT  // 0-1023

static bool backlight_initialized = false;

/**
 * @brief 初始化背光PWM
 */
static esp_err_t tft_backlight_init(void)
{
    // 配置PWM定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = TFT_BL_PWM_MODE,
        .duty_resolution  = TFT_BL_PWM_DUTY_RES,
        .timer_num        = TFT_BL_PWM_TIMER,
        .freq_hz          = TFT_BL_PWM_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 配置PWM通道
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = TFT_PIN_BL,
        .speed_mode     = TFT_BL_PWM_MODE,
        .channel        = TFT_BL_PWM_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = TFT_BL_PWM_TIMER,
        .duty           = 0,  // 初始为0(关闭)
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    backlight_initialized = true;
    ESP_LOGI(TAG, "背光PWM初始化成功");
    return ESP_OK;
}

esp_err_t tft_display_set_backlight(uint8_t brightness)
{
    if (!backlight_initialized) {
        ESP_LOGE(TAG, "背光PWM未初始化");
        return ESP_FAIL;
    }

    if (brightness > 100) {
        brightness = 100;
    }

    // 计算PWM占空比 (0-1023)
    uint32_t duty = (brightness * 1023) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(TFT_BL_PWM_MODE, TFT_BL_PWM_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(TFT_BL_PWM_MODE, TFT_BL_PWM_CHANNEL));

    ESP_LOGI(TAG, "背光亮度设置为 %d%%", brightness);
    return ESP_OK;
}

esp_err_t tft_display_init(esp_lcd_panel_handle_t *panel_handle)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "开始初始化TFT显示屏 (ILI9341)");
    ESP_LOGI(TAG, "========================================");

    // ==================== 配置SPI总线 ====================
    // 打印内部RAM状态
    ESP_LOGI(TAG, "初始化前内部RAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));

    spi_bus_config_t buscfg = {
        .mosi_io_num = TFT_PIN_MOSI,
        .miso_io_num = -1,  // ILI9341单向传输，不需要MISO
        .sclk_io_num = TFT_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_H_RES * 40 * sizeof(uint16_t),  // 减小到40行(19.2KB)
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI总线初始化成功 (max_transfer_sz=%d bytes)", buscfg.max_transfer_sz);
    ESP_LOGI(TAG, "初始化后内部RAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));

    // ==================== 配置LCD面板IO ====================
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = TFT_PIN_DC,
        .cs_gpio_num = TFT_PIN_CS,
        .pclk_hz = TFT_SPI_FREQ_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TFT_SPI_HOST, &io_config, &io_handle));
    ESP_LOGI(TAG, "LCD面板IO初始化成功");

    // ==================== 配置LCD面板 ====================
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // ILI9341使用BGR顺序
        .bits_per_pixel = TFT_BIT_PER_PIXEL,
    };
    // 使用ST7789驱动 (ILI9341兼容)
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, panel_handle));
    ESP_LOGI(TAG, "LCD面板驱动初始化成功");

    // ==================== 初始化面板 ====================
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_handle));

    // 设置屏幕方向（竖屏）
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(*panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(*panel_handle, true, false));  // 修复文字镜像：X轴镜像

    // 打开显示
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*panel_handle, true));
    ESP_LOGI(TAG, "LCD面板已打开");

    // ==================== 初始化背光 ====================
    ESP_ERROR_CHECK(tft_backlight_init());
    ESP_ERROR_CHECK(tft_display_set_backlight(80));  // 默认80%亮度

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "TFT显示屏初始化完成!");
    ESP_LOGI(TAG, "分辨率: %dx%d", TFT_H_RES, TFT_V_RES);
    ESP_LOGI(TAG, "========================================");

    return ESP_OK;
}

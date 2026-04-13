/**
 * @file i2s_audio.c
 * @brief I2S音频驱动实现
 */

#include "i2s_audio.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "I2S_AUDIO";

// I2S通道句柄
static i2s_chan_handle_t tx_handle = NULL;  // 播放(功放)
static i2s_chan_handle_t rx_handle = NULL;  // 录音(麦克风)

/**
 * @brief 初始化I2S音频系统
 */
esp_err_t i2s_audio_init(void)
{
    esp_err_t ret = ESP_OK;

    // ==================== 配置I2S_NUM_0 - 麦克风录音 ====================
    i2s_chan_config_t rx_chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DMA_BUF_COUNT,
        .dma_frame_num = DMA_BUF_LEN,
        .auto_clear = true,
    };

    ret = i2s_new_channel(&rx_chan_cfg, NULL, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建I2S RX通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_MIC_BCLK,
            .ws = I2S_MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_MIC_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(rx_handle, &rx_std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化I2S RX标准模式失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用I2S RX通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "麦克风I2S初始化成功 (BCLK=%d, WS=%d, DIN=%d)",
             I2S_MIC_BCLK, I2S_MIC_WS, I2S_MIC_DIN);

    // ==================== 配置I2S_NUM_1 - 功放播放 ====================
    i2s_chan_config_t tx_chan_cfg = {
        .id = I2S_NUM_1,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = DMA_BUF_COUNT,
        .dma_frame_num = DMA_BUF_LEN,
        .auto_clear = true,
    };

    ret = i2s_new_channel(&tx_chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建I2S TX通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SPK_BCLK,
            .ws = I2S_SPK_WS,
            .dout = I2S_SPK_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_handle, &tx_std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化I2S TX标准模式失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启用I2S TX通道失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "功放I2S初始化成功 (BCLK=%d, WS=%d, DOUT=%d)",
             I2S_SPK_BCLK, I2S_SPK_WS, I2S_SPK_DOUT);

    return ESP_OK;
}

/**
 * @brief 录制音频
 */
esp_err_t i2s_record_audio(uint8_t *buffer, size_t buffer_size, size_t *bytes_read)
{
    if (buffer == NULL || bytes_read == NULL) {
        ESP_LOGE(TAG, "参数错误: buffer或bytes_read为NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (buffer_size < RECORD_BUFFER_SIZE) {
        ESP_LOGE(TAG, "缓冲区太小: 需要%d字节, 提供%d字节",
                 RECORD_BUFFER_SIZE, buffer_size);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "开始录音 (时长=%d秒, 大小=%d字节)...",
             RECORD_DURATION_SEC, RECORD_BUFFER_SIZE);

    size_t total_read = 0;
    esp_err_t ret = ESP_OK;

    // 分块读取音频数据
    while (total_read < RECORD_BUFFER_SIZE) {
        size_t bytes_to_read = RECORD_BUFFER_SIZE - total_read;
        if (bytes_to_read > DMA_BUF_LEN * 2) {
            bytes_to_read = DMA_BUF_LEN * 2;
        }

        size_t bytes_read_chunk = 0;
        ret = i2s_channel_read(rx_handle, buffer + total_read,
                               bytes_to_read, &bytes_read_chunk,
                               portMAX_DELAY);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "读取I2S数据失败: %s", esp_err_to_name(ret));
            *bytes_read = total_read;
            return ret;
        }

        total_read += bytes_read_chunk;
    }

    *bytes_read = total_read;
    ESP_LOGI(TAG, "录音完成: %d字节", total_read);

    return ESP_OK;
}

/**
 * @brief 播放音频
 */
esp_err_t i2s_play_audio(const uint8_t *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        ESP_LOGE(TAG, "参数错误: buffer为NULL或大小为0");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "开始播放音频 (%d字节)...", buffer_size);

    size_t total_written = 0;
    esp_err_t ret = ESP_OK;

    // 分块写入音频数据
    while (total_written < buffer_size) {
        size_t bytes_to_write = buffer_size - total_written;
        if (bytes_to_write > DMA_BUF_LEN * 2) {
            bytes_to_write = DMA_BUF_LEN * 2;
        }

        size_t bytes_written_chunk = 0;
        ret = i2s_channel_write(tx_handle, buffer + total_written,
                                bytes_to_write, &bytes_written_chunk,
                                portMAX_DELAY);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "写入I2S数据失败: %s", esp_err_to_name(ret));
            return ret;
        }

        total_written += bytes_written_chunk;
    }

    ESP_LOGI(TAG, "播放完成: %d字节", total_written);

    return ESP_OK;
}

/**
 * @brief 反初始化I2S音频系统
 */
esp_err_t i2s_audio_deinit(void)
{
    esp_err_t ret = ESP_OK;

    if (rx_handle != NULL) {
        i2s_channel_disable(rx_handle);
        i2s_del_channel(rx_handle);
        rx_handle = NULL;
    }

    if (tx_handle != NULL) {
        i2s_channel_disable(tx_handle);
        i2s_del_channel(tx_handle);
        tx_handle = NULL;
    }

    ESP_LOGI(TAG, "I2S音频系统已关闭");

    return ret;
}

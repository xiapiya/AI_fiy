/**
 * @file i2s_audio.h
 * @brief I2S音频驱动 - 麦克风录音和功放播放
 *
 * 硬件配置:
 * - 麦克风: INMP441 (I2S_NUM_0)
 * - 功放: MAX98357A (I2S_NUM_1)
 * - 采样率: 8000Hz
 * - 位深: 16bit
 * - 声道: 单声道(Mono)
 */

#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// ==================== 硬件引脚定义 ====================
// 麦克风 (INMP441) - I2S_NUM_0
#define I2S_MIC_BCLK   GPIO_NUM_7
#define I2S_MIC_WS     GPIO_NUM_8
#define I2S_MIC_DIN    GPIO_NUM_9

// 功放 (MAX98357A) - I2S_NUM_1
#define I2S_SPK_BCLK   GPIO_NUM_2
#define I2S_SPK_WS     GPIO_NUM_1
#define I2S_SPK_DOUT   GPIO_NUM_4

// ==================== 音频参数 ====================
#define SAMPLE_RATE         8000    // 采样率 8kHz
#define SAMPLE_BITS         16      // 16位采样
#define CHANNELS            1       // 单声道
#define RECORD_DURATION_SEC 3       // 录音时长3秒

// 录音缓冲区大小 (3秒音频)
#define RECORD_BUFFER_SIZE  (SAMPLE_RATE * CHANNELS * (SAMPLE_BITS / 8) * RECORD_DURATION_SEC)

// I2S DMA缓冲区配置
#define DMA_BUF_COUNT       8
#define DMA_BUF_LEN         1024

/**
 * @brief 初始化I2S音频系统
 *
 * 初始化麦克风和功放的I2S接口
 *
 * @return ESP_OK 成功
 *         其他错误码 失败
 */
esp_err_t i2s_audio_init(void);

/**
 * @brief 录制音频
 *
 * 从INMP441麦克风录制3秒音频数据
 *
 * @param[out] buffer 音频数据缓冲区(需要由调用者分配)
 * @param[in] buffer_size 缓冲区大小(字节)
 * @param[out] bytes_read 实际读取的字节数
 *
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数错误
 *         其他错误码 录音失败
 */
esp_err_t i2s_record_audio(uint8_t *buffer, size_t buffer_size, size_t *bytes_read);

/**
 * @brief 播放音频
 *
 * 通过MAX98357A功放播放PCM音频数据
 *
 * @param[in] buffer 音频数据缓冲区
 * @param[in] buffer_size 音频数据大小(字节)
 *
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数错误
 *         其他错误码 播放失败
 */
esp_err_t i2s_play_audio(const uint8_t *buffer, size_t buffer_size);

/**
 * @brief 反初始化I2S音频系统
 *
 * 释放I2S资源
 *
 * @return ESP_OK 成功
 */
esp_err_t i2s_audio_deinit(void);

#endif // I2S_AUDIO_H

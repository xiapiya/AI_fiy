/**
 * @file vad.c
 * @brief VAD语音活动检测实现
 *
 * 基于能量阈值的简单VAD算法
 * 复用EMQX官方blog_4的VAD逻辑
 */

#include "vad.h"
#include <math.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "VAD";

// VAD内部状态
static struct {
    float threshold;           // 能量阈值
    int trigger_count;         // 连续触发计数
    bool is_triggered;         // 是否已触发
} vad_state = {
    .threshold = VAD_ENERGY_THRESHOLD,
    .trigger_count = 0,
    .is_triggered = false,
};

/**
 * @brief 初始化VAD检测器
 */
void vad_init(float threshold)
{
    if (threshold > 0) {
        vad_state.threshold = threshold;
    } else {
        vad_state.threshold = VAD_ENERGY_THRESHOLD;
    }

    vad_state.trigger_count = 0;
    vad_state.is_triggered = false;

    ESP_LOGI(TAG, "VAD初始化完成 (阈值=%.2f, 触发帧数=%d)",
             vad_state.threshold, VAD_TRIGGER_FRAMES);
}

/**
 * @brief 计算音频能量 (RMS)
 */
float vad_calculate_energy(const int16_t *audio_frame, size_t frame_size)
{
    if (audio_frame == NULL || frame_size == 0) {
        return 0.0f;
    }

    // 计算均方根(RMS)能量
    float sum = 0.0f;
    for (size_t i = 0; i < frame_size; i++) {
        float sample = (float)audio_frame[i];
        sum += sample * sample;
    }

    float rms = sqrtf(sum / frame_size);
    return rms;
}

/**
 * @brief 检测音频帧是否包含语音
 */
bool vad_detect(const int16_t *audio_frame, size_t frame_size)
{
    if (audio_frame == NULL || frame_size == 0) {
        return false;
    }

    // 计算当前帧能量
    float energy = vad_calculate_energy(audio_frame, frame_size);

    // 判断是否超过阈值
    if (energy > vad_state.threshold) {
        vad_state.trigger_count++;
        ESP_LOGD(TAG, "检测到高能量: %.2f (触发计数=%d/%d)",
                 energy, vad_state.trigger_count, VAD_TRIGGER_FRAMES);

        // 连续N帧超过阈值,触发录音
        if (vad_state.trigger_count >= VAD_TRIGGER_FRAMES && !vad_state.is_triggered) {
            vad_state.is_triggered = true;
            ESP_LOGI(TAG, "VAD触发! 开始录音...");
            return true;
        }
    } else {
        // 能量低于阈值,重置计数
        if (vad_state.trigger_count > 0) {
            ESP_LOGD(TAG, "能量下降: %.2f (重置计数)", energy);
        }
        vad_state.trigger_count = 0;
    }

    return false;
}

/**
 * @brief 重置VAD状态
 */
void vad_reset(void)
{
    vad_state.trigger_count = 0;
    vad_state.is_triggered = false;
    ESP_LOGD(TAG, "VAD状态已重置");
}

/**
 * @brief 设置能量阈值
 */
void vad_set_threshold(float threshold)
{
    if (threshold > 0) {
        vad_state.threshold = threshold;
        ESP_LOGI(TAG, "VAD阈值已更新: %.2f", threshold);
    }
}

/**
 * @brief 获取当前能量阈值
 */
float vad_get_threshold(void)
{
    return vad_state.threshold;
}

/**
 * @file vad.h
 * @brief VAD语音活动检测 - 能量阈值检测
 *
 * 基于EMQX官方blog_4的VAD逻辑，使用ESP-IDF C实现
 * 检测音频能量，当超过阈值时触发录音
 */

#ifndef VAD_H
#define VAD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ==================== VAD配置参数 ====================
#define VAD_SAMPLE_RATE      8000    // 采样率8kHz
#define VAD_FRAME_SIZE       512     // 每帧采样数
#define VAD_FRAME_DURATION_MS ((VAD_FRAME_SIZE * 1000) / VAD_SAMPLE_RATE)  // 帧时长(毫秒)

// 能量阈值(需要根据实际环境调整)
#define VAD_ENERGY_THRESHOLD 500.0f  // 默认阈值500

// 触发条件: 连续N帧超过阈值
#define VAD_TRIGGER_FRAMES   3

/**
 * @brief 初始化VAD检测器
 *
 * 设置能量阈值和触发参数
 *
 * @param[in] threshold 能量阈值(可选,0表示使用默认值)
 */
void vad_init(float threshold);

/**
 * @brief 检测音频帧是否包含语音
 *
 * 计算音频帧的能量,判断是否超过阈值
 *
 * @param[in] audio_frame 音频帧数据(16bit PCM)
 * @param[in] frame_size 帧大小(采样点数)
 *
 * @return true 检测到语音
 *         false 无语音
 */
bool vad_detect(const int16_t *audio_frame, size_t frame_size);

/**
 * @brief 计算音频能量
 *
 * 计算音频帧的RMS能量值
 *
 * @param[in] audio_frame 音频帧数据(16bit PCM)
 * @param[in] frame_size 帧大小(采样点数)
 *
 * @return 能量值(float)
 */
float vad_calculate_energy(const int16_t *audio_frame, size_t frame_size);

/**
 * @brief 重置VAD状态
 *
 * 清除内部计数器,重新开始检测
 */
void vad_reset(void);

/**
 * @brief 设置能量阈值
 *
 * 动态调整阈值(用于自适应)
 *
 * @param[in] threshold 新的能量阈值
 */
void vad_set_threshold(float threshold);

/**
 * @brief 获取当前能量阈值
 *
 * @return 当前阈值
 */
float vad_get_threshold(void);

#endif // VAD_H

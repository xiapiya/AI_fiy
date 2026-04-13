/**
 * @file state_machine.h
 * @brief 系统状态机 - 基于FreeRTOS Event Group
 *
 * 管理设备运行状态,包括:
 * - 待机(IDLE)
 * - 录音(RECORDING)
 * - TLS握手(TLS_HANDSHAKE)
 * - 云端同步(CLOUD_SYNC)
 * - 播放(PLAYING)
 * - 冷却期(COOLDOWN)
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// ==================== 状态位定义 ====================
// V3.2基础状态
#define STATE_IDLE_BIT          BIT0   // 待机状态
#define STATE_RECORDING_BIT     BIT1   // 录音中
#define STATE_PLAYING_BIT       BIT2   // 播放中
#define STATE_COOLDOWN_BIT      BIT3   // 冷却期(3秒)

// V4.0新增公网状态
#define STATE_TLS_HANDSHAKE_BIT BIT4   // TLS握手中
#define STATE_CLOUD_SYNC_BIT    BIT5   // 云端同步中

// 互斥状态组 (同时只能有一个)
#define STATE_MUTEX_MASK (STATE_IDLE_BIT | STATE_RECORDING_BIT | \
                          STATE_PLAYING_BIT | STATE_COOLDOWN_BIT | \
                          STATE_TLS_HANDSHAKE_BIT | STATE_CLOUD_SYNC_BIT)

// ==================== 状态机初始化 ====================
/**
 * @brief 初始化状态机
 *
 * 创建FreeRTOS Event Group并设置初始状态为IDLE
 */
void state_machine_init(void);

/**
 * @brief 获取Event Group句柄
 *
 * @return EventGroupHandle_t 事件组句柄
 */
EventGroupHandle_t state_machine_get_event_group(void);

// ==================== 状态转换函数 ====================
/**
 * @brief 设置状态
 *
 * 清除当前互斥状态,设置新状态
 *
 * @param[in] state_bit 新状态位
 */
void state_machine_set_state(EventBits_t state_bit);

/**
 * @brief 获取当前状态
 *
 * @return 当前状态位
 */
EventBits_t state_machine_get_state(void);

/**
 * @brief 检查是否处于某个状态
 *
 * @param[in] state_bit 要检查的状态位
 *
 * @return true 处于该状态
 *         false 不处于该状态
 */
bool state_machine_is_state(EventBits_t state_bit);

/**
 * @brief 等待状态变化
 *
 * 阻塞等待状态变为指定状态
 *
 * @param[in] state_bit 等待的状态位
 * @param[in] timeout_ms 超时时间(毫秒),0表示永久等待
 *
 * @return true 状态已变化
 *         false 超时
 */
bool state_machine_wait_state(EventBits_t state_bit, uint32_t timeout_ms);

// ==================== 状态转换快捷函数 ====================
/**
 * @brief 进入待机状态
 */
void state_machine_enter_idle(void);

/**
 * @brief 进入录音状态
 */
void state_machine_enter_recording(void);

/**
 * @brief 进入TLS握手状态
 */
void state_machine_enter_tls_handshake(void);

/**
 * @brief 进入云端同步状态
 */
void state_machine_enter_cloud_sync(void);

/**
 * @brief 进入播放状态
 */
void state_machine_enter_playing(void);

/**
 * @brief 进入冷却期
 *
 * 播放完成后强制进入3秒冷却期
 */
void state_machine_enter_cooldown(void);

/**
 * @brief 获取状态名称字符串
 *
 * @param[in] state_bit 状态位
 *
 * @return 状态名称字符串
 */
const char* state_machine_get_state_name(EventBits_t state_bit);

#endif // STATE_MACHINE_H

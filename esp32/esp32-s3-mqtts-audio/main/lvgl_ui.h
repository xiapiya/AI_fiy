/**
 * @file lvgl_ui.h
 * @brief LVGL情感化UI界面
 *
 * 基于LVGL v9.x实现的情感表情显示系统
 * - 顶部: 网络状态栏 (WiFi + MQTT图标)
 * - 中间: 情绪表情动画
 * - 底部: 滚动字幕
 */

#ifndef LVGL_UI_H
#define LVGL_UI_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

// ==================== 情绪类型定义 ====================
typedef enum {
    EMOTION_NEUTRAL,   // 中性
    EMOTION_HAPPY,     // 开心
    EMOTION_SAD,       // 难过
    EMOTION_COMFORT,   // 安慰
    EMOTION_THINKING   // 思考中
} emotion_t;

/**
 * @brief 初始化LVGL UI
 *
 * 配置LVGL、创建UI组件、启动刷新任务（含Mutex保护）
 *
 * @param[in] panel_handle LCD面板句柄
 * @return ESP_OK 成功
 *         ESP_FAIL 失败
 */
esp_err_t lvgl_ui_init(esp_lcd_panel_handle_t panel_handle);

/**
 * @brief 设置情绪表情
 *
 * 线程安全，内部使用Mutex保护
 *
 * @param[in] emotion 情绪类型
 */
void lvgl_ui_set_emotion(emotion_t emotion);

/**
 * @brief 设置字幕文本
 *
 * 线程安全，内部使用Mutex保护
 *
 * @param[in] text 字幕内容（UTF-8编码）
 */
void lvgl_ui_set_text(const char *text);

/**
 * @brief 设置WiFi连接状态
 *
 * @param[in] connected true=已连接, false=断开
 */
void lvgl_ui_set_wifi_status(bool connected);

/**
 * @brief 设置MQTT连接状态
 *
 * @param[in] connected true=已连接, false=断开
 */
void lvgl_ui_set_mqtt_status(bool connected);

#endif // LVGL_UI_H

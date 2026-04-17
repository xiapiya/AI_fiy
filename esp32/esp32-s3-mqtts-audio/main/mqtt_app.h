/**
 * @file mqtt_app.h
 * @brief MQTTS客户端 - 加密通信和消息收发
 *
 * 功能:
 * - 连接云端EMQX Broker (mqtts://端口8883)
 * - TLS/SSL加密通信
 * - 自动断线重连
 * - Base64音频数据上传/下载
 */

#ifndef MQTT_APP_H
#define MQTT_APP_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// ==================== MQTT主题定义 ====================
// 注意: {product}和{device_id}需要在运行时替换
#define MQTT_TOPIC_AUDIO_UPLOAD    "aifly/esp32/%s/audio"      // 上传音频
#define MQTT_TOPIC_AUDIO_PLAY      "aifly/esp32/%s/playaudio"  // 下发音频
#define MQTT_TOPIC_STATUS          "aifly/esp32/%s/status"     // 设备状态
#define MQTT_TOPIC_CONTROL         "aifly/esp32/%s/control"    // 远程控制

// ==================== MQTT配置参数 ====================
// 警告: 这是示例配置，实际使用时必须修改！
// 1. 修改 MQTT_BROKER_URI 为实际的云端EMQX地址
// 2. 修改 MQTT_USERNAME 和 MQTT_PASSWORD 为实际的MQTT认证信息
// 3. 修改 MQTT_CLIENT_ID 为唯一的设备ID
// 4. 替换 certs/ca.pem 为实际的服务器CA证书
#define MQTT_BROKER_URI    "mqtts://103.73.161.244:8883"  // 需要修改为实际云端地址
#define MQTT_USERNAME      "xiapiyaesp32"                  // 需要修改为实际用户名
#define MQTT_PASSWORD      "wzj123456"                 // 需要修改为实际密码
#define MQTT_CLIENT_ID     "xiapiya_s3_001"                  // 设备ID (建议使用MAC地址)

#define MQTT_KEEPALIVE_SEC     60   // 心跳间隔60秒
#define MQTT_RECONNECT_TIMEOUT_MS 5000  // 重连超时5秒

// ==================== 回调函数类型定义 ====================
/**
 * @brief 音频下发回调函数
 *
 * 当收到云端下发的音频数据时调用
 *
 * @param[in] audio_data Base64解码后的PCM音频数据
 * @param[in] data_len 音频数据长度(字节)
 */
typedef void (*mqtt_audio_received_cb_t)(const uint8_t *audio_data, size_t data_len);

/**
 * @brief 控制指令回调函数
 *
 * 当收到云端控制指令时调用
 *
 * @param[in] command 控制指令字符串
 */
typedef void (*mqtt_control_received_cb_t)(const char *command);

// ==================== MQTT客户端初始化 ====================
/**
 * @brief 初始化MQTTS客户端
 *
 * 连接云端EMQX Broker，启用TLS加密
 *
 * @param[in] audio_cb 音频下发回调函数
 * @param[in] control_cb 控制指令回调函数
 *
 * @return ESP_OK 成功
 *         其他错误码 失败
 */
esp_err_t mqtt_app_init(mqtt_audio_received_cb_t audio_cb,
                        mqtt_control_received_cb_t control_cb);

/**
 * @brief 上传音频数据到云端
 *
 * 将PCM音频数据Base64编码后上传到MQTT主题
 *
 * @param[in] audio_data PCM音频数据
 * @param[in] data_len 数据长度(字节)
 *
 * @return ESP_OK 成功
 *         ESP_ERR_NO_MEM 内存不足
 *         其他错误码 上传失败
 */
esp_err_t mqtt_publish_audio(const uint8_t *audio_data, size_t data_len);

/**
 * @brief 上报设备状态到云端
 *
 * 发送设备状态JSON到status主题
 *
 * @param[in] status_json JSON格式的状态字符串
 *
 * @return ESP_OK 成功
 *         其他错误码 失败
 */
esp_err_t mqtt_publish_status(const char *status_json);

/**
 * @brief 检查MQTT连接状态
 *
 * @return true 已连接
 *         false 未连接
 */
bool mqtt_is_connected(void);

/**
 * @brief 停止MQTT客户端
 */
void mqtt_app_stop(void);

#endif // MQTT_APP_H

/**
 * @file mqtt_app.c
 * @brief MQTTS客户端实现
 */

#include "mqtt_app.h"
#include "i2s_audio.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "mbedtls/base64.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "MQTT_CLIENT";

// 外部声明嵌入的CA证书
extern const uint8_t ca_pem_start[] asm("_binary_ca_pem_start");
extern const uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

// MQTT客户端句柄
static esp_mqtt_client_handle_t mqtt_client = NULL;

// 回调函数指针
static mqtt_audio_received_cb_t audio_callback = NULL;
static mqtt_control_received_cb_t control_callback = NULL;

// 音频解码缓冲区(动态分配到PSRAM)
static uint8_t *audio_decode_buffer = NULL;

// MQTT主题字符串缓冲区
static char topic_audio_upload[128];
static char topic_audio_play[128];
static char topic_status[128];
static char topic_control[128];

// 连接状态标志
static bool mqtt_connected = false;

/**
 * @brief Base64解码音频数据
 *
 * @param[in] input Base64编码的字符串
 * @param[in] input_len 输入长度
 * @param[out] output 解码后的数据缓冲区
 * @param[out] output_len 解码后的数据长度
 *
 * @return ESP_OK 成功
 */
static esp_err_t base64_decode_audio(const char *input, size_t input_len,
                                     uint8_t *output, size_t *output_len)
{
    size_t decoded_len = 0;
    int ret = mbedtls_base64_decode(output, *output_len, &decoded_len,
                                    (const unsigned char *)input, input_len);

    if (ret == 0) {
        *output_len = decoded_len;
        return ESP_OK;
    } else if (ret == MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) {
        ESP_LOGE(TAG, "Base64解码缓冲区太小");
        return ESP_ERR_NO_MEM;
    } else {
        ESP_LOGE(TAG, "Base64解码失败: %d", ret);
        return ESP_FAIL;
    }
}

/**
 * @brief MQTT事件处理函数
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT连接成功!");
            mqtt_connected = true;

            // 订阅主题
            esp_mqtt_client_subscribe(mqtt_client, topic_audio_play, 1);
            esp_mqtt_client_subscribe(mqtt_client, topic_control, 1);
            ESP_LOGI(TAG, "已订阅主题: %s, %s", topic_audio_play, topic_control);

            // 上报在线状态
            mqtt_publish_status("{\"status\":\"online\",\"version\":\"v4.0\"}");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT断开连接");
            mqtt_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "订阅成功, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "收到MQTT消息: topic=%.*s, data_len=%d",
                     event->topic_len, event->topic, event->data_len);

            // 处理音频下发消息
            if (strncmp(event->topic, topic_audio_play, event->topic_len) == 0) {
                if (audio_callback != NULL && audio_decode_buffer != NULL) {
                    // Base64解码音频数据
                    size_t decoded_len = RECORD_BUFFER_SIZE * 2;

                    esp_err_t ret = base64_decode_audio(event->data, event->data_len,
                                                        audio_decode_buffer, &decoded_len);
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "音频解码成功: %d字节", decoded_len);
                        audio_callback(audio_decode_buffer, decoded_len);
                    } else {
                        ESP_LOGE(TAG, "音频解码失败");
                    }
                }
            }
            // 处理控制指令
            else if (strncmp(event->topic, topic_control, event->topic_len) == 0) {
                if (control_callback != NULL) {
                    // 复制控制指令字符串(确保null结尾)
                    static char cmd_buffer[256];
                    size_t copy_len = (event->data_len < sizeof(cmd_buffer) - 1) ?
                                      event->data_len : sizeof(cmd_buffer) - 1;
                    memcpy(cmd_buffer, event->data, copy_len);
                    cmd_buffer[copy_len] = '\0';

                    ESP_LOGI(TAG, "收到控制指令: %s", cmd_buffer);
                    control_callback(cmd_buffer);
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT错误事件");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "TCP传输错误: 0x%x", event->error_handle->esp_transport_sock_errno);
            }
            break;

        default:
            ESP_LOGD(TAG, "其他MQTT事件: id=%d", event_id);
            break;
    }
}

/**
 * @brief 初始化MQTTS客户端
 */
esp_err_t mqtt_app_init(mqtt_audio_received_cb_t audio_cb,
                        mqtt_control_received_cb_t control_cb)
{
    // 保存回调函数
    audio_callback = audio_cb;
    control_callback = control_cb;

    // 分配音频解码缓冲区到PSRAM (预留2倍空间用于Base64解码)
    audio_decode_buffer = heap_caps_malloc(RECORD_BUFFER_SIZE * 2, MALLOC_CAP_SPIRAM);
    if (audio_decode_buffer == NULL) {
        ESP_LOGE(TAG, "分配音频解码缓冲区失败! 需要%d字节", RECORD_BUFFER_SIZE * 2);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "音频解码缓冲区分配成功: %d字节 (PSRAM)", RECORD_BUFFER_SIZE * 2);

    // 生成MQTT主题字符串
    snprintf(topic_audio_upload, sizeof(topic_audio_upload),
             MQTT_TOPIC_AUDIO_UPLOAD, MQTT_CLIENT_ID);
    snprintf(topic_audio_play, sizeof(topic_audio_play),
             MQTT_TOPIC_AUDIO_PLAY, MQTT_CLIENT_ID);
    snprintf(topic_status, sizeof(topic_status),
             MQTT_TOPIC_STATUS, MQTT_CLIENT_ID);
    snprintf(topic_control, sizeof(topic_control),
             MQTT_TOPIC_CONTROL, MQTT_CLIENT_ID);

    ESP_LOGI(TAG, "MQTT主题配置:");
    ESP_LOGI(TAG, "  上传音频: %s", topic_audio_upload);
    ESP_LOGI(TAG, "  下发音频: %s", topic_audio_play);
    ESP_LOGI(TAG, "  设备状态: %s", topic_status);
    ESP_LOGI(TAG, "  远程控制: %s", topic_control);

    // 配置MQTT客户端 (ESP-IDF 5.x格式)
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .broker.verification.certificate = (const char *)ca_pem_start,
        .credentials.client_id = MQTT_CLIENT_ID,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session.keepalive = MQTT_KEEPALIVE_SEC,
        .network.reconnect_timeout_ms = MQTT_RECONNECT_TIMEOUT_MS,
        .network.disable_auto_reconnect = false,
    };

    ESP_LOGI(TAG, "正在连接MQTTS Broker: %s", MQTT_BROKER_URI);
    ESP_LOGI(TAG, "客户端ID: %s", MQTT_CLIENT_ID);

    // 创建MQTT客户端
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "创建MQTT客户端失败!");
        return ESP_FAIL;
    }

    // 注册事件处理函数
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    // 启动MQTT客户端
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动MQTT客户端失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MQTT客户端启动成功");
    return ESP_OK;
}

/**
 * @brief 上传音频数据到云端
 */
esp_err_t mqtt_publish_audio(const uint8_t *audio_data, size_t data_len)
{
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT未连接,无法上传音频");
        return ESP_ERR_INVALID_STATE;
    }

    // 计算Base64编码后的大小
    size_t encoded_len = 0;
    mbedtls_base64_encode(NULL, 0, &encoded_len, audio_data, data_len);

    // 分配Base64缓冲区到PSRAM (编码后数据较大)
    char *base64_buffer = heap_caps_malloc(encoded_len + 1, MALLOC_CAP_SPIRAM);
    if (base64_buffer == NULL) {
        ESP_LOGE(TAG, "分配Base64缓冲区失败 (需要%d字节)", encoded_len);
        return ESP_ERR_NO_MEM;
    }

    // Base64编码
    size_t actual_len = 0;
    int ret = mbedtls_base64_encode((unsigned char *)base64_buffer, encoded_len,
                                    &actual_len, audio_data, data_len);

    if (ret != 0) {
        ESP_LOGE(TAG, "Base64编码失败: %d", ret);
        heap_caps_free(base64_buffer);
        return ESP_FAIL;
    }

    base64_buffer[actual_len] = '\0';
    ESP_LOGI(TAG, "音频编码完成: 原始=%d字节, Base64=%d字节", data_len, actual_len);

    // 发布到MQTT主题
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic_audio_upload,
                                         base64_buffer, actual_len, 1, 0);

    heap_caps_free(base64_buffer);

    if (msg_id < 0) {
        ESP_LOGE(TAG, "发布MQTT消息失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "音频上传成功, msg_id=%d", msg_id);
    return ESP_OK;
}

/**
 * @brief 上报设备状态到云端
 */
esp_err_t mqtt_publish_status(const char *status_json)
{
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT未连接,无法上报状态");
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic_status,
                                         status_json, strlen(status_json), 0, 0);

    if (msg_id < 0) {
        ESP_LOGE(TAG, "发布状态消息失败");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "状态上报成功: %s", status_json);
    return ESP_OK;
}

/**
 * @brief 检查MQTT连接状态
 */
bool mqtt_is_connected(void)
{
    return mqtt_connected;
}

/**
 * @brief 停止MQTT客户端
 */
void mqtt_app_stop(void)
{
    if (mqtt_client != NULL) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        mqtt_connected = false;
        ESP_LOGI(TAG, "MQTT客户端已停止");
    }

    // 释放音频解码缓冲区
    if (audio_decode_buffer != NULL) {
        heap_caps_free(audio_decode_buffer);
        audio_decode_buffer = NULL;
        ESP_LOGI(TAG, "音频解码缓冲区已释放");
    }
}

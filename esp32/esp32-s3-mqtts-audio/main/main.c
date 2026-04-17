/**
 * @file main.c
 * @brief ESP32-S3 MQTTS音频通信主程序
 *
 * V4.0云端架构版
 * - 纯ESP-IDF开发
 * - MQTTS加密通信
 * - VAD能量检测
 * - FreeRTOS状态机
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_psram.h"

#include "i2s_audio.h"
#include "mqtt_app.h"
#include "vad.h"
#include "state_machine.h"
#include "tft_display.h"
#include "lvgl_ui.h"

static const char *TAG = "MAIN";

// ==================== WiFi配置 ====================
// 警告: 这是示例配置，实际使用时必须修改！
// 1. 修改 WIFI_SSID 为实际的WiFi网络名称
// 2. 修改 WIFI_PASSWORD 为实际的WiFi密码
// 3. 确保WiFi网络可以访问互联网(MQTTS需要公网连接)
#define WIFI_SSID      "C510"      // 需要修改为实际WiFi SSID
#define WIFI_PASSWORD  "71377137"  // 需要修改为实际WiFi密码
#define WIFI_MAXIMUM_RETRY  5

static int wifi_retry_count = 0;
    static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT      = BIT1;

// ==================== 全局缓冲区 ====================
// 使用PSRAM存储大数据缓冲区
static uint8_t *audio_record_buffer = NULL;

// ==================== WiFi事件处理 ====================
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_count < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            wifi_retry_count++;
            ESP_LOGI(TAG, "重试连接WiFi (%d/%d)", wifi_retry_count, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "WiFi连接失败");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "获取IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief 初始化WiFi
 */
static esp_err_t wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi初始化完成");

    // 等待连接成功或失败
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi连接成功: SSID=%s", WIFI_SSID);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi连接失败: SSID=%s", WIFI_SSID);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WiFi连接异常");
        return ESP_FAIL;
    }
}

// ==================== MQTT回调函数 ====================
/**
 * @brief 音频下发回调
 */
static void on_audio_received(const uint8_t *audio_data, size_t data_len)
{
    ESP_LOGI(TAG, "收到云端音频数据: %d字节", data_len);

    // 进入播放状态
    state_machine_enter_playing();

    // 播放音频
    esp_err_t ret = i2s_play_audio(audio_data, data_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "播放音频失败");
    }

    // 进入冷却期
    state_machine_enter_cooldown();
    ESP_LOGI(TAG, "进入冷却期 (3秒)...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // 返回待机状态
    state_machine_enter_idle();
    ESP_LOGI(TAG, "冷却期结束,返回待机状态");
}

/**
 * @brief 控制指令回调
 */
static void on_control_received(const char *command)
{
    ESP_LOGI(TAG, "收到控制指令: %s", command);

    // 处理简单的控制指令
    if (strcmp(command, "mute") == 0) {
        ESP_LOGI(TAG, "执行静音指令");
        // TODO: 实现静音功能
    } else if (strcmp(command, "unmute") == 0) {
        ESP_LOGI(TAG, "执行取消静音指令");
        // TODO: 实现取消静音功能
    } else {
        ESP_LOGW(TAG, "未知控制指令: %s", command);
    }
}

// ==================== VAD监听任务 ====================
/**
 * @brief VAD监听任务
 *
 * 持续监听麦克风输入,检测语音活动
 */
static void vad_task(void *pvParameters)
{
    ESP_LOGI(TAG, "VAD监听任务启动");

    // 音频帧缓冲区
    int16_t *frame_buffer = malloc(VAD_FRAME_SIZE * sizeof(int16_t));
    if (frame_buffer == NULL) {
        ESP_LOGE(TAG, "分配VAD帧缓冲区失败!");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        // 只在待机状态监听
        if (!state_machine_is_state(STATE_IDLE_BIT)) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 读取一帧音频数据
        size_t bytes_read = 0;
        esp_err_t ret = i2s_record_audio((uint8_t *)frame_buffer,
                                         VAD_FRAME_SIZE * sizeof(int16_t),
                                         &bytes_read);

        if (ret != ESP_OK || bytes_read == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // VAD检测
        if (vad_detect(frame_buffer, VAD_FRAME_SIZE)) {
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, ">>> 录音开始 <<<");
            ESP_LOGI(TAG, "========================================");

            // 进入录音状态
            state_machine_enter_recording();

            // 录制3秒音频
            size_t total_bytes = 0;
            ret = i2s_record_audio(audio_record_buffer, RECORD_BUFFER_SIZE, &total_bytes);

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "========================================");
                ESP_LOGI(TAG, ">>> 录音结束: %d字节 (%.1f秒) <<<",
                         total_bytes, (float)total_bytes / (SAMPLE_RATE * 2));
                ESP_LOGI(TAG, "========================================");

                // 进入云端同步状态
                state_machine_enter_cloud_sync();

                ESP_LOGI(TAG, ">>> 开始上传音频到服务器...");
                // 上传音频到云端
                ret = mqtt_publish_audio(audio_record_buffer, total_bytes);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, ">>> 音频上传成功! <<<");
                } else {
                    ESP_LOGE(TAG, ">>> 音频上传失败! <<<");
                }
            } else {
                ESP_LOGE(TAG, "录音失败");
            }

            // 重置VAD状态
            vad_reset();

            // 返回待机状态
            state_machine_enter_idle();
        }

        // 短暂延时
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    free(frame_buffer);
    vTaskDelete(NULL);
}

// ==================== 主函数 ====================
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-S3 MQTTS音频通信系统 V4.0");
    ESP_LOGI(TAG, "========================================");

    // ==================== 初始化NVS ====================
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ==================== 初始化PSRAM ====================
    ret = esp_psram_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PSRAM初始化失败!");
        return;
    }

    ESP_LOGI(TAG, "内存状态:");
    ESP_LOGI(TAG, "  Free heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Free internal: %d bytes", esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "  PSRAM size: %d bytes", esp_psram_get_size());

    // 分配音频缓冲区到PSRAM
    audio_record_buffer = heap_caps_malloc(RECORD_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (audio_record_buffer == NULL) {
        ESP_LOGE(TAG, "分配音频缓冲区失败!");
        return;
    }
    ESP_LOGI(TAG, "音频缓冲区分配成功: %d bytes (PSRAM)", RECORD_BUFFER_SIZE);

    // ==================== 初始化WiFi ====================
    ret = wifi_init_sta();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi初始化失败,系统停止");
        return;
    }

    // ==================== 初始化状态机 ====================
    state_machine_init();

    // ==================== 初始化I2S音频 ====================
    ret = i2s_audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S初始化失败,系统停止");
        return;
    }

    // ==================== 初始化TFT显示屏 ====================
    esp_lcd_panel_handle_t panel_handle = NULL;
    ret = tft_display_init(&panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TFT初始化失败");
        // TFT失败不影响核心功能，继续运行
    } else {
        // ==================== 初始化LVGL UI ====================
        ret = lvgl_ui_init(panel_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LVGL初始化失败");
        } else {
            // 设置WiFi连接状态
            lvgl_ui_set_wifi_status(true);
        }
    }

    // ==================== 初始化VAD ====================
    vad_init(1500.0);  // 能量阈值(安静时200-1000,说话时>2000)

    // ==================== 初始化MQTT ====================
    state_machine_enter_tls_handshake();
    ESP_LOGI(TAG, "正在建立TLS连接...");

    ret = mqtt_app_init(on_audio_received, on_control_received);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT初始化失败,系统停止");
        return;
    }

    // 等待MQTT连接成功
    int retry = 0;
    while (!mqtt_is_connected() && retry < 30) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
    }

    if (mqtt_is_connected()) {
        ESP_LOGI(TAG, "MQTT连接成功!");
        state_machine_enter_idle();
        lvgl_ui_set_mqtt_status(true);
    } else {
        ESP_LOGE(TAG, "MQTT连接超时");
        lvgl_ui_set_mqtt_status(false);
        return;
    }

    // ==================== 创建VAD监听任务 ====================
    xTaskCreate(vad_task, "VAD_Task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "系统启动完成,进入待机状态");
    ESP_LOGI(TAG, "========================================");

    // 主任务进入空闲循环
    while (1) {
        // 定期上报系统状态
        if (mqtt_is_connected()) {
            char status_buf[128];
            snprintf(status_buf, sizeof(status_buf),
                     "{\"state\":\"%s\",\"heap\":%lu}",
                     state_machine_get_state_name(state_machine_get_state()),
                     (unsigned long)esp_get_free_heap_size());
            mqtt_publish_status(status_buf);
        }

        vTaskDelay(pdMS_TO_TICKS(30000));  // 每30秒上报一次
    }
}

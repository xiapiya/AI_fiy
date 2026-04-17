/**
 * @file lvgl_ui.c
 * @brief LVGL情感化UI实现
 *
 * 使用LVGL v9实现情绪表情显示系统
 */

#include "lvgl_ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "tft_display.h"
#include "state_machine.h"

static const char *TAG = "LVGL_UI";

// ==================== LVGL全局变量 ====================
static lv_display_t *lvgl_display = NULL;
static SemaphoreHandle_t lvgl_mutex = NULL;

// ==================== UI组件 ====================
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *mqtt_icon = NULL;
static lv_obj_t *emotion_obj = NULL;
static lv_obj_t *subtitle_label = NULL;

// ==================== 状态变量 ====================
static emotion_t current_emotion = EMOTION_NEUTRAL;
static bool wifi_connected = false;
static bool mqtt_connected = false;

// ==================== LVGL显示刷新回调 ====================
/**
 * @brief LVGL显示刷新回调
 */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // 发送数据到LCD
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    // 通知LVGL刷新完成
    lv_display_flush_ready(disp);
}

// ==================== 创建UI组件 ====================
/**
 * @brief 创建状态栏
 */
static void create_status_bar(void)
{
    // 创建状态栏容器
    lv_obj_t *status_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(status_bar, TFT_H_RES, 30);
    lv_obj_set_pos(status_bar, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);  // 清除所有padding

    // WiFi状态标签
    wifi_icon = lv_label_create(status_bar);
    lv_label_set_text(wifi_icon, "WiFi:OFF");
    lv_obj_set_pos(wifi_icon, 5, 5);  // Y坐标从7调整为5
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xFF0000), 0);

    // MQTT状态标签
    mqtt_icon = lv_label_create(status_bar);
    lv_label_set_text(mqtt_icon, "MQTT:OFF");
    lv_obj_set_pos(mqtt_icon, 100, 5);  // Y坐标从7调整为5
    lv_obj_set_style_text_color(mqtt_icon, lv_color_hex(0xFF0000), 0);

    ESP_LOGI(TAG, "状态栏创建完成");
}

/**
 * @brief 创建情绪表情
 */
static void create_emotion_display(void)
{
    // 创建中间容器
    lv_obj_t *emotion_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(emotion_container, TFT_H_RES, 200);
    lv_obj_set_pos(emotion_container, 0, 40);
    lv_obj_set_style_bg_color(emotion_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(emotion_container, 0, 0);
    lv_obj_set_style_radius(emotion_container, 0, 0);
    lv_obj_set_style_pad_all(emotion_container, 0, 0);  // 清除所有padding

    // 创建情绪表情对象（使用圆形）
    emotion_obj = lv_obj_create(emotion_container);
    lv_obj_set_size(emotion_obj, 100, 100);
    lv_obj_align(emotion_obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(emotion_obj, lv_color_hex(0xFFFFFF), 0);  // 默认白色(中性)
    lv_obj_set_style_border_width(emotion_obj, 0, 0);
    lv_obj_set_style_pad_all(emotion_obj, 0, 0);  // 清除圆形的padding

    ESP_LOGI(TAG, "情绪表情创建完成");
}

/**
 * @brief 创建字幕区域
 */
static void create_subtitle_area(void)
{
    // 创建字幕容器
    lv_obj_t *subtitle_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(subtitle_container, TFT_H_RES, 80);
    lv_obj_set_pos(subtitle_container, 0, TFT_V_RES - 80);
    lv_obj_set_style_bg_color(subtitle_container, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(subtitle_container, 0, 0);
    lv_obj_set_style_radius(subtitle_container, 0, 0);
    lv_obj_set_style_pad_all(subtitle_container, 10, 0);  // 设置10px padding用于文字边距

    // 创建字幕标签
    subtitle_label = lv_label_create(subtitle_container);
    lv_obj_set_width(subtitle_label, TFT_H_RES - 20);
    lv_obj_align(subtitle_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(subtitle_label, "System Ready");
    lv_obj_set_style_text_color(subtitle_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(subtitle_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(subtitle_label, &lv_font_montserrat_20, 0);
    lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_WRAP);  // 自动换行

    ESP_LOGI(TAG, "字幕区域创建完成");
}

// ==================== LVGL任务 ====================
/**
 * @brief LVGL刷新任务
 */
static void lvgl_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LVGL刷新任务启动");

    uint32_t tick_count = 0;

    while (1) {
        // 获取锁
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            uint32_t time_till_next = lv_timer_handler();  // 处理LVGL定时器

            // 每100次打印一次调试信息
            if (tick_count % 100 == 0) {
                ESP_LOGD(TAG, "LVGL刷新 tick=%lu, next=%lu ms", tick_count, time_till_next);
            }
            tick_count++;

            xSemaphoreGive(lvgl_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(5));  // 200Hz刷新率
    }
}

/**
 * @brief 状态监听任务
 */
static void state_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "状态监听任务启动");

    EventGroupHandle_t state_event_group = state_machine_get_event_group();
    EventBits_t last_state = 0;

    while (1) {
        // 获取当前状态
        EventBits_t current_state = xEventGroupGetBits(state_event_group);

        // 只在状态变化时更新UI
        if (current_state != last_state) {
            ESP_LOGI(TAG, "状态变化检测: 0x%02X -> 0x%02X", last_state, current_state);

            // 根据状态更新UI
            if (current_state & STATE_IDLE_BIT) {
                ESP_LOGI(TAG, "进入IDLE状态,更新UI");
                lvgl_ui_set_emotion(EMOTION_NEUTRAL);
                lvgl_ui_set_text("Listening...");
            } else if (current_state & STATE_RECORDING_BIT) {
                ESP_LOGI(TAG, "进入RECORDING状态,更新UI");
                lvgl_ui_set_emotion(EMOTION_HAPPY);
                lvgl_ui_set_text("Recording...");
            } else if (current_state & STATE_CLOUD_SYNC_BIT) {
                ESP_LOGI(TAG, "进入CLOUD_SYNC状态,更新UI");
                lvgl_ui_set_emotion(EMOTION_THINKING);
                lvgl_ui_set_text("Thinking...");
            } else if (current_state & STATE_PLAYING_BIT) {
                ESP_LOGI(TAG, "进入PLAYING状态,更新UI");
                lvgl_ui_set_emotion(EMOTION_COMFORT);
                lvgl_ui_set_text("Playing...");
            } else if (current_state & STATE_TLS_HANDSHAKE_BIT) {
                ESP_LOGI(TAG, "进入TLS_HANDSHAKE状态,更新UI");
                lvgl_ui_set_emotion(EMOTION_THINKING);
                lvgl_ui_set_text("Connecting...");
            }

            last_state = current_state;
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // 每100ms检查一次状态变化
    }
}

// ==================== 公共接口实现 ====================
esp_err_t lvgl_ui_init(esp_lcd_panel_handle_t panel_handle)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "开始初始化LVGL UI");
    ESP_LOGI(TAG, "========================================");

    // 创建互斥锁
    lvgl_mutex = xSemaphoreCreateMutex();
    if (lvgl_mutex == NULL) {
        ESP_LOGE(TAG, "创建LVGL互斥锁失败");
        return ESP_FAIL;
    }

    // 初始化LVGL
    lv_init();
    ESP_LOGI(TAG, "LVGL核心初始化完成");

    // 分配显示缓冲区到PSRAM (双缓冲, 1/10屏幕)
    size_t buffer_size = TFT_H_RES * 32 * sizeof(lv_color_t);  // 32行缓冲
    lv_color_t *buf1 = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);

    if (buf1 == NULL || buf2 == NULL) {
        ESP_LOGE(TAG, "分配LVGL缓冲区失败 (需要 %d bytes x2)", buffer_size);
        if (buf1) free(buf1);
        if (buf2) free(buf2);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "LVGL双缓冲区分配成功: %d bytes x2 (PSRAM)", buffer_size);

    // 创建显示设备
    lvgl_display = lv_display_create(TFT_H_RES, TFT_V_RES);
    lv_display_set_flush_cb(lvgl_display, lvgl_flush_cb);
    lv_display_set_buffers(lvgl_display, buf1, buf2, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(lvgl_display, panel_handle);
    ESP_LOGI(TAG, "LVGL显示设备创建完成");

    // 创建UI组件
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    // 设置背景色
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);

    create_status_bar();
    create_emotion_display();
    create_subtitle_area();

    xSemaphoreGive(lvgl_mutex);
    ESP_LOGI(TAG, "UI组件创建完成");

    // 创建LVGL刷新任务
    xTaskCreate(lvgl_task, "LVGL_Task", 8192, NULL, 2, NULL);
    ESP_LOGI(TAG, "LVGL刷新任务已创建");

    // 创建状态监听任务
    xTaskCreate(state_monitor_task, "State_Monitor", 4096, NULL, 2, NULL);
    ESP_LOGI(TAG, "状态监听任务已创建");

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "LVGL UI初始化完成!");
    ESP_LOGI(TAG, "========================================");

    return ESP_OK;
}

void lvgl_ui_set_emotion(emotion_t emotion)
{
    if (emotion_obj == NULL) {
        ESP_LOGW(TAG, "情绪对象未初始化,跳过更新");
        return;
    }

    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_emotion = emotion;

        // 根据情绪设置颜色
        switch (emotion) {
            case EMOTION_NEUTRAL:
                lv_obj_set_style_bg_color(emotion_obj, lv_color_hex(0xFFFFFF), 0);  // 白色
                ESP_LOGI(TAG, "情绪已更新: NEUTRAL (白色)");
                break;
            case EMOTION_HAPPY:
                lv_obj_set_style_bg_color(emotion_obj, lv_color_hex(0xFFD700), 0);  // 金色
                ESP_LOGI(TAG, "情绪已更新: HAPPY (金色)");
                break;
            case EMOTION_SAD:
                lv_obj_set_style_bg_color(emotion_obj, lv_color_hex(0x4169E1), 0);  // 蓝色
                ESP_LOGI(TAG, "情绪已更新: SAD (蓝色)");
                break;
            case EMOTION_COMFORT:
                lv_obj_set_style_bg_color(emotion_obj, lv_color_hex(0x32CD32), 0);  // 绿色
                ESP_LOGI(TAG, "情绪已更新: COMFORT (绿色)");
                break;
            case EMOTION_THINKING:
                lv_obj_set_style_bg_color(emotion_obj, lv_color_hex(0x808080), 0);  // 灰色
                ESP_LOGI(TAG, "情绪已更新: THINKING (灰色)");
                break;
        }

        lv_obj_invalidate(emotion_obj);  // 标记需要重绘
        lv_refr_now(lvgl_display);       // 立即强制刷新显示

        xSemaphoreGive(lvgl_mutex);
    } else {
        ESP_LOGE(TAG, "获取LVGL互斥锁失败!");
    }
}

void lvgl_ui_set_text(const char *text)
{
    if (subtitle_label == NULL || text == NULL) {
        ESP_LOGW(TAG, "字幕标签未初始化或文本为空,跳过更新");
        return;
    }

    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lv_label_set_text(subtitle_label, text);
        ESP_LOGI(TAG, "字幕已更新: %s", text);

        lv_obj_invalidate(subtitle_label);  // 标记需要重绘
        lv_refr_now(lvgl_display);          // 立即强制刷新显示

        xSemaphoreGive(lvgl_mutex);
    } else {
        ESP_LOGE(TAG, "获取LVGL互斥锁失败!");
    }
}

void lvgl_ui_set_wifi_status(bool connected)
{
    if (wifi_icon == NULL) {
        ESP_LOGW(TAG, "WiFi图标未初始化,跳过更新");
        return;
    }

    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        wifi_connected = connected;

        if (connected) {
            lv_label_set_text(wifi_icon, "WiFi:ON");
            lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x00FF00), 0);  // 绿色(已连接)
            ESP_LOGI(TAG, "WiFi状态已更新: ON (绿色)");
        } else {
            lv_label_set_text(wifi_icon, "WiFi:OFF");
            lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xFF0000), 0);  // 红色(未连接)
            ESP_LOGI(TAG, "WiFi状态已更新: OFF (红色)");
        }

        lv_obj_invalidate(wifi_icon);  // 标记需要重绘
        lv_refr_now(lvgl_display);     // 立即强制刷新显示

        xSemaphoreGive(lvgl_mutex);
    } else {
        ESP_LOGE(TAG, "获取LVGL互斥锁失败!");
    }
}

void lvgl_ui_set_mqtt_status(bool connected)
{
    if (mqtt_icon == NULL) {
        ESP_LOGW(TAG, "MQTT图标未初始化,跳过更新");
        return;
    }

    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        mqtt_connected = connected;

        if (connected) {
            lv_label_set_text(mqtt_icon, "MQTT:ON");
            lv_obj_set_style_text_color(mqtt_icon, lv_color_hex(0x00FF00), 0);  // 绿色(已连接)
            ESP_LOGI(TAG, "MQTT状态已更新: ON (绿色)");
        } else {
            lv_label_set_text(mqtt_icon, "MQTT:OFF");
            lv_obj_set_style_text_color(mqtt_icon, lv_color_hex(0xFF0000), 0);  // 红色(未连接)
            ESP_LOGI(TAG, "MQTT状态已更新: OFF (红色)");
        }

        lv_obj_invalidate(mqtt_icon);  // 标记需要重绘
        lv_refr_now(lvgl_display);     // 立即强制刷新显示

        xSemaphoreGive(lvgl_mutex);
    } else {
        ESP_LOGE(TAG, "获取LVGL互斥锁失败!");
    }
}

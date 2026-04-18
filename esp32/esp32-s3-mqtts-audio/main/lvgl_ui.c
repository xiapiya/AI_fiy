/**
 * @file lvgl_ui.c
 * @brief LVGL情感化UI实现 (集成SquareLine Studio生成的UI)
 *
 * 使用LVGL v9 + SquareLine Studio实现情绪表情显示系统
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

// 引入SquareLine Studio生成的UI
#include "AIUI/ui.h"
#include "AIUI/screens/ui_MainScreen.h"

static const char *TAG = "LVGL_UI";

// ==================== LVGL全局变量 ====================
static lv_display_t *lvgl_display = NULL;
static SemaphoreHandle_t lvgl_mutex = NULL;

// ==================== 状态变量 ====================
static emotion_t current_emotion = EMOTION_NEUTRAL;
static bool wifi_connected = false;
static bool mqtt_connected = false;

// ==================== UI状态枚举 ====================
typedef enum {
    UI_STATE_LISTENING = 0,  // 倾听模式：静态圆形 + 灰色 + "..."
    UI_STATE_SPEAKING  = 1,  // 说话模式：呼吸动画 + 绿色
    UI_STATE_THINKING  = 2,  // 思考模式：呼吸动画 + 金色
    UI_STATE_CAPTURING = 3,  // 拍照模式：呼吸动画 + 蓝色
} ui_emotion_state_t;

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

// ==================== UI更新函数 ====================
/**
 * @brief 更新网络状态图标
 *
 * @param wifi_ok WiFi连接状态 (true=已连接, false=断开)
 * @param mqtt_ok MQTT连接状态 (true=已连接, false=断开)
 */
static void update_network_status(bool wifi_ok, bool mqtt_ok)
{
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // WiFi状态: 绿色=正常, 灰色=异常
        lv_obj_set_style_text_color(
            ui_label_wifi1,
            wifi_ok ? lv_color_hex(0x00FF00) : lv_color_hex(0x808080),
            0
        );

        // MQTT状态: 蓝色=正常, 灰色=异常
        lv_obj_set_style_text_color(
            ui_label_mqtt1,
            mqtt_ok ? lv_color_hex(0x00A8FF) : lv_color_hex(0x808080),
            0
        );

        // 强制刷新显示
        lv_obj_invalidate(ui_label_wifi1);
        lv_obj_invalidate(ui_label_mqtt1);
        lv_refr_now(lvgl_display);

        ESP_LOGI(TAG, "网络状态已更新: WiFi=%s, MQTT=%s",
                 wifi_ok ? "ON" : "OFF",
                 mqtt_ok ? "ON" : "OFF");

        xSemaphoreGive(lvgl_mutex);
    } else {
        ESP_LOGE(TAG, "获取LVGL互斥锁失败，网络状态更新被跳过");
    }
}

/**
 * @brief 切换情绪状态UI
 *
 * @param state UI状态（倾听/说话/思考/拍照）
 */
static void change_emotion_state(ui_emotion_state_t state)
{
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // CRITICAL: 先停止所有动画，再启动新动画
        lv_anim_del(ui_EmotionCircle1, NULL);

        switch (state) {
            case UI_STATE_LISTENING:
                // 倾听模式：静态圆形
                lv_obj_set_width(ui_EmotionCircle1, 120);
                lv_obj_set_height(ui_EmotionCircle1, 120);
                lv_obj_set_style_bg_color(ui_EmotionCircle1, lv_color_hex(0xD3D3D3), 0);
                lv_label_set_text(ui_uiEmotionLabel1, "...");
                ESP_LOGI(TAG, "UI状态: 倾听模式");
                break;

            case UI_STATE_SPEAKING:
                // 说话模式：呼吸动画 + 绿色
                lv_label_set_text(ui_uiEmotionLabel1, "");
                lv_obj_set_style_bg_color(ui_EmotionCircle1, lv_color_hex(0x32CD32), 0);
                breathe_Animation(ui_EmotionCircle1, 0);
                ESP_LOGI(TAG, "UI状态: 说话模式");
                break;

            case UI_STATE_THINKING:
                // 思考模式：呼吸动画 + 金色
                lv_label_set_text(ui_uiEmotionLabel1, "");
                lv_obj_set_style_bg_color(ui_EmotionCircle1, lv_color_hex(0xFFD700), 0);
                breathe_Animation(ui_EmotionCircle1, 0);
                ESP_LOGI(TAG, "UI状态: 思考模式");
                break;

            case UI_STATE_CAPTURING:
                // 拍照模式：呼吸动画 + 蓝色
                lv_label_set_text(ui_uiEmotionLabel1, "");
                lv_obj_set_style_bg_color(ui_EmotionCircle1, lv_color_hex(0x00A8FF), 0);
                breathe_Animation(ui_EmotionCircle1, 0);
                ESP_LOGI(TAG, "UI状态: 拍照模式");
                break;
        }

        // 强制刷新显示
        lv_obj_invalidate(ui_EmotionCircle1);
        lv_obj_invalidate(ui_uiEmotionLabel1);
        lv_refr_now(lvgl_display);

        xSemaphoreGive(lvgl_mutex);
    } else {
        ESP_LOGE(TAG, "获取LVGL互斥锁失败，UI状态更新被跳过");
    }
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

            // 根据状态机状态更新UI
            if (current_state & STATE_IDLE_BIT) {
                ESP_LOGI(TAG, "进入IDLE状态,更新UI");
                change_emotion_state(UI_STATE_LISTENING);
            }
            else if (current_state & (STATE_RECORDING_BIT | STATE_PLAYING_BIT)) {
                ESP_LOGI(TAG, "进入RECORDING/PLAYING状态,更新UI");
                change_emotion_state(UI_STATE_SPEAKING);
            }
            else if (current_state & (STATE_CLOUD_SYNC_BIT | STATE_TLS_HANDSHAKE_BIT)) {
                ESP_LOGI(TAG, "进入THINKING状态,更新UI");
                change_emotion_state(UI_STATE_THINKING);
            }
            // TODO: 拍照功能未实现，暂时注释
            // else if (current_state & STATE_CAPTURING_BIT) {
            //     ESP_LOGI(TAG, "进入CAPTURING状态,更新UI");
            //     change_emotion_state(UI_STATE_CAPTURING);
            // }

            last_state = current_state;
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // 每100ms检查一次状态变化
    }
}

// ==================== 公共接口实现 ====================
esp_err_t lvgl_ui_init(esp_lcd_panel_handle_t panel_handle)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "开始初始化LVGL UI (SquareLine Studio)");
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

    // 初始化SquareLine Studio生成的UI
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    ui_init();  // ← SquareLine生成的初始化函数
    xSemaphoreGive(lvgl_mutex);
    ESP_LOGI(TAG, "SquareLine Studio UI初始化完成");

    // 验证UI组件是否存在
    if (ui_EmotionCircle1 == NULL) {
        ESP_LOGE(TAG, "ui_EmotionCircle1未初始化！");
        return ESP_FAIL;
    }
    if (ui_uiEmotionLabel1 == NULL) {
        ESP_LOGE(TAG, "ui_uiEmotionLabel1未初始化！");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "UI组件验证通过");

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
    if (ui_EmotionCircle1 == NULL) {
        ESP_LOGW(TAG, "情绪对象未初始化,跳过更新");
        return;
    }

    // 映射emotion_t到ui_emotion_state_t
    ui_emotion_state_t ui_state;
    switch (emotion) {
        case EMOTION_NEUTRAL:
            ui_state = UI_STATE_LISTENING;
            break;
        case EMOTION_HAPPY:
        case EMOTION_COMFORT:
            ui_state = UI_STATE_SPEAKING;
            break;
        case EMOTION_THINKING:
            ui_state = UI_STATE_THINKING;
            break;
        case EMOTION_SAD:
            ui_state = UI_STATE_CAPTURING;  // 使用蓝色
            break;
        default:
            ui_state = UI_STATE_LISTENING;
            break;
    }

    current_emotion = emotion;
    change_emotion_state(ui_state);
}

void lvgl_ui_set_text(const char *text)
{
    // SquareLine Studio生成的UI中没有字幕标签
    // 暂时记录日志，后续可在SquareLine中添加字幕组件
    ESP_LOGI(TAG, "字幕文本: %s (暂无UI组件)", text ? text : "NULL");
}

void lvgl_ui_set_wifi_status(bool connected)
{
    if (ui_label_wifi1 == NULL) {
        ESP_LOGW(TAG, "WiFi图标未初始化,跳过更新");
        return;
    }

    wifi_connected = connected;
    update_network_status(wifi_connected, mqtt_connected);
}

void lvgl_ui_set_mqtt_status(bool connected)
{
    if (ui_label_mqtt1 == NULL) {
        ESP_LOGW(TAG, "MQTT图标未初始化,跳过更新");
        return;
    }

    mqtt_connected = connected;
    update_network_status(wifi_connected, mqtt_connected);
}

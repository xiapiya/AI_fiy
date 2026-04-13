/**
 * @file state_machine.c
 * @brief 系统状态机实现
 */

#include "state_machine.h"
#include "esp_log.h"

static const char *TAG = "STATE_MACHINE";

// FreeRTOS Event Group句柄
static EventGroupHandle_t system_event_group = NULL;

/**
 * @brief 初始化状态机
 */
void state_machine_init(void)
{
    // 创建Event Group
    system_event_group = xEventGroupCreate();
    if (system_event_group == NULL) {
        ESP_LOGE(TAG, "创建Event Group失败!");
        return;
    }

    // 设置初始状态为IDLE
    xEventGroupSetBits(system_event_group, STATE_IDLE_BIT);
    ESP_LOGI(TAG, "状态机初始化完成 (初始状态=IDLE)");
}

/**
 * @brief 获取Event Group句柄
 */
EventGroupHandle_t state_machine_get_event_group(void)
{
    return system_event_group;
}

/**
 * @brief 设置状态
 */
void state_machine_set_state(EventBits_t state_bit)
{
    if (system_event_group == NULL) {
        ESP_LOGE(TAG, "状态机未初始化!");
        return;
    }

    // 清除所有互斥状态位
    xEventGroupClearBits(system_event_group, STATE_MUTEX_MASK);

    // 设置新状态
    xEventGroupSetBits(system_event_group, state_bit);

    ESP_LOGI(TAG, "状态切换: %s", state_machine_get_state_name(state_bit));
}

/**
 * @brief 获取当前状态
 */
EventBits_t state_machine_get_state(void)
{
    if (system_event_group == NULL) {
        return 0;
    }

    EventBits_t bits = xEventGroupGetBits(system_event_group);
    return bits & STATE_MUTEX_MASK;
}

/**
 * @brief 检查是否处于某个状态
 */
bool state_machine_is_state(EventBits_t state_bit)
{
    EventBits_t current = state_machine_get_state();
    return (current & state_bit) != 0;
}

/**
 * @brief 等待状态变化
 */
bool state_machine_wait_state(EventBits_t state_bit, uint32_t timeout_ms)
{
    if (system_event_group == NULL) {
        return false;
    }

    TickType_t ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    EventBits_t bits = xEventGroupWaitBits(
        system_event_group,
        state_bit,
        pdFALSE,  // 不清除位
        pdFALSE,  // 等待任意一位
        ticks
    );

    return (bits & state_bit) != 0;
}

// ==================== 状态转换快捷函数 ====================
void state_machine_enter_idle(void)
{
    state_machine_set_state(STATE_IDLE_BIT);
}

void state_machine_enter_recording(void)
{
    state_machine_set_state(STATE_RECORDING_BIT);
}

void state_machine_enter_tls_handshake(void)
{
    state_machine_set_state(STATE_TLS_HANDSHAKE_BIT);
}

void state_machine_enter_cloud_sync(void)
{
    state_machine_set_state(STATE_CLOUD_SYNC_BIT);
}

void state_machine_enter_playing(void)
{
    state_machine_set_state(STATE_PLAYING_BIT);
}

void state_machine_enter_cooldown(void)
{
    state_machine_set_state(STATE_COOLDOWN_BIT);
}

/**
 * @brief 获取状态名称字符串
 */
const char* state_machine_get_state_name(EventBits_t state_bit)
{
    switch (state_bit) {
        case STATE_IDLE_BIT:
            return "IDLE";
        case STATE_RECORDING_BIT:
            return "RECORDING";
        case STATE_TLS_HANDSHAKE_BIT:
            return "TLS_HANDSHAKE";
        case STATE_CLOUD_SYNC_BIT:
            return "CLOUD_SYNC";
        case STATE_PLAYING_BIT:
            return "PLAYING";
        case STATE_COOLDOWN_BIT:
            return "COOLDOWN";
        default:
            return "UNKNOWN";
    }
}

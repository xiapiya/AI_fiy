# ESP32-S3 Embedded Development Guidelines (V3.2 纯 ESP-IDF)

> 嵌入式硬件开发规范(基于PRD/Epic/Story V3.2)

---

## Overview

**硬件平台**: ESP32-S3-N16R8 (16MB Flash + 8MB PSRAM)
**开发框架**: **纯 ESP-IDF (v5.x)** - **全面弃用 Arduino 框架**
**核心功能**: I2S音频 + SPI TFT + OV2640摄像头 + MQTT + HTTP + LVGL UI
**任务调度**: FreeRTOS 原生多任务调度

**核心原则**:
1. **禁止造轮子** - 优先复用官方代码并用 ESP-IDF API 重写
2. **工业级稳定** - 使用原生 ESP-IDF API 而非 Arduino 封装
3. **FreeRTOS 管理** - 严格使用任务队列、信号量、互斥锁

---

## 必须复用的官方代码（ESP-IDF 重写策略）

⚠️ **重要**: EMQX 官方代码基于 Arduino 框架，**不可直接复制代码**。需要**提炼业务逻辑**，使用 ESP-IDF 原生 API 重写。

### 1. 音频处理 + MQTT (blog_4) - ESP-IDF 重写

**参考仓库**: `emqx/esp32-mcp-mqtt-tutorial/samples/blog_4`

**复用业务逻辑** (用 ESP-IDF C 重写):
- **I2S 音频录制**: 使用 `driver/i2s_std.h` (IDF v5.x) 替代 Arduino I2S
  - 引脚: INMP441 (BCLK=7, LRCL=8, DOUT=9)
  - FreeRTOS 任务: `xTaskCreate()` 创建独立录音任务
  - 环形缓冲: `RingBuffer` 存储到 PSRAM
- **I2S 音频播放**: 使用 `i2s_channel_write()` 驱动 MAX98357A
  - 引脚: MAX98357A (BCLK=2, LRCL=1, DOUT=42)
- **VAD 能量检测**: 保留原逻辑，用 C 实现阈值检测
- **MQTT 客户端**: 使用 `mqtt/mqtt_client.h` 替代 PubSubClient
- **Base64 编解码**: 使用 `mbedtls/base64.h`

**禁止**:
- ❌ 不要使用 Arduino I2S 库
- ❌ 不要使用 PubSubClient (Arduino 库)
- ❌ 不要跳过 FreeRTOS 任务设计

### 2. 摄像头 + HTTP 上传 (blog_6) - ESP-IDF 重写

**参考仓库**: `emqx/esp32-mcp-mqtt-tutorial/samples/blog_6`

**复用业务逻辑**:
- **OV2640 驱动**: 使用官方 `esp32-camera` 组件 (C 驱动)
- **JPEG 压缩**: 使用 esp32-camera 的内置 JPEG 编码器
- **HTTP POST**: 使用 `esp_http_client.h` 上传图片
- **拍照触发**: MQTT 订阅控制主题

**禁止**:
- ❌ 不要使用 Arduino 的简单摄像头封装
- ❌ 不要自己写 JPEG 编码器

### 3. TFT 屏幕 + LVGL - ESP-IDF 官方示例

**参考**: ESP-IDF 官方 `esp_lcd` 移植示例

**必须使用**:
- **屏幕驱动**: `esp_lcd_panel_*` API 驱动 SPI TFT
- **UI 库**: LVGL v9.6
- **性能优化**:
  - `CONFIG_SPIRAM_MODE_OCT=y` (启用 PSRAM 八线模式)
  - `CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y` (LVGL IRAM 加速)
  - CPU 240MHz + DMA 传输

**禁止**:
- ❌ 不要使用 TFT_eSPI (Arduino 库)
- ❌ 不要忘记 LVGL 线程安全 (必须用 `xSemaphoreTake/Give` 加锁)

---

## FreeRTOS 任务架构

### 核心任务设计

```c
// 使用 FreeRTOS 任务创建
void app_main(void) {
    // 任务1: VAD 监听 + I2S 录音
    xTaskCreate(vad_task, "VAD_Task", 4096, NULL, 5, NULL);

    // 任务2: I2S 音频播放
    xTaskCreate(audio_play_task, "Play_Task", 4096, NULL, 4, NULL);

    // 任务3: MQTT 通信
    xTaskCreate(mqtt_task, "MQTT_Task", 8192, NULL, 3, NULL);

    // 任务4: LVGL UI 刷新 (必须加锁!)
    xTaskCreate(lvgl_task, "LVGL_Task", 8192, NULL, 2, NULL);

    // 任务5: 摄像头抓拍 (按需触发)
    xTaskCreate(camera_task, "Camera_Task", 4096, NULL, 3, NULL);
}
```

### 状态机 (基于 FreeRTOS Event Group)

```c
// 使用事件组管理状态
EventGroupHandle_t system_event_group;

#define STATE_IDLE_BIT       BIT0   // 待机
#define STATE_RECORDING_BIT  BIT1   // 录音中
#define STATE_CAPTURING_BIT  BIT2   // 拍照中
#define STATE_THINKING_BIT   BIT3   // 推理中
#define STATE_PLAYING_BIT    BIT4   // 播放中
#define STATE_COOLDOWN_BIT   BIT5   // 冷却期(3秒)
```

### PRD 核心要求

- **播放后强制冷却 3秒**: 使用 `vTaskDelay(pdMS_TO_TICKS(3000))` 屏蔽麦克风
- **FreeRTOS 同步**: 所有任务通过 Queue/Semaphore/EventGroup 通信
- **MQTT 断线重连**: `esp_mqtt_client` 内置自动重连
- **LVGL 线程安全**: 所有 UI 操作必须加锁 `xSemaphoreTake(lvgl_mutex, portMAX_DELAY)`

---

## LVGL UI (esp_lcd + LVGL v9.6)

**驱动**: ESP-IDF 原生 `esp_lcd_panel_*` API (替代 TFT_eSPI)
**UI 库**: LVGL v9.6
**性能优化**:
- 启用 PSRAM 全帧缓冲 (`CONFIG_SPIRAM_MODE_OCT=y`)
- IRAM 加速 (`CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y`)
- CPU 240MHz + DMA 传输
- Double Buffer 防撕裂

### 线程安全 (CRITICAL!)

```c
// 必须创建 LVGL 互斥锁
SemaphoreHandle_t lvgl_mutex;

// UI 刷新任务
void lvgl_task(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
            lv_task_handler();  // LVGL 核心刷新
            xSemaphoreGive(lvgl_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 其他任务更新 UI 时也必须加锁
void update_ui_from_other_task() {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    lv_label_set_text(label, "新文本");
    xSemaphoreGive(lvgl_mutex);
}
```

### 表情映射 (情绪标签 → UI 动画)

| 情绪标签 | 表情资源 | 触发条件 | UI 动作 |
|---------|---------|---------|---------|
| happy | happy.png (预编译为 C 数组) | AI 回复包含 "开心" | 切换表情 + 滚动字幕 |
| sad | sad.png | AI 回复包含 "难过" | 切换表情 + 滚动字幕 |
| comfort | comfort.png | AI 回复包含 "理解"、"陪伴" | 切换表情 + 滚动字幕 |
| thinking | loading.gif | 推理中 | "神经元链接中..." 动画 |

**图片资源处理**:
- 使用 LVGL 的图片转换工具生成 C 数组
- 存储到 Flash，运行时加载到 PSRAM

---

## MQTT Topics (分层主题设计)

```
{product}/esp32/{device_id}/audio       # ESP32 上传录音 (Base64)
{product}/esp32/{device_id}/playaudio   # 云端下发播放音频 (MP3 Base64)
{product}/esp32/{device_id}/ui          # 云端下发 UI 状态 (JSON: text/emotion/status)
{product}/esp32/{device_id}/control     # 云端/App 控制指令 (远程拍照、静音)
{product}/esp32/{device_id}/status      # ESP32 上报设备状态 (心跳、错误)
emqx/system/logs                        # 全局日志主题
```

### esp_mqtt_client 示例

```c
#include "mqtt_client.h"

esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = "mqtt://your-broker:1883",
    .credentials.client_id = "esp32_device_001",
};

esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
esp_mqtt_client_start(client);

// 订阅主题
esp_mqtt_client_subscribe(client, "product/esp32/001/playaudio", 1);
esp_mqtt_client_subscribe(client, "product/esp32/001/ui", 1);
```

---

## 核心原则

### ✅ DO

- 优先复用官方blog_4+blog_6代码
- 严格遵循状态机,避免状态混乱
- 播放后强制3秒冷却
- MQTT断线自动重连
- **所有代码的注释必须使用中文**

### ❌ DON'T

- ❌ 不要自己写I2S/摄像头驱动
- ❌ 不要跳过冷却期(会导致回声)
- ❌ 不要在PSRAM<8MB的芯片上跑LVGL全帧缓冲
- ❌ **写完代码之后不需要写README.md等文档**

---

**总结**: 复用官方代码 + 状态机管理 + LVGL UI + 性能优化

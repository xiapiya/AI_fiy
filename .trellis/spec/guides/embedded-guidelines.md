# ESP32-S3 Embedded Development Guidelines (V4.0 云端架构演进版)

> 嵌入式硬件开发规范(基于PRD/Epic/Story V4.0)
>
> **版本演进**: V4.0在V3.2基础上新增公网加密通信、云端容器化架构、多端协同能力

---

## Overview

**硬件平台**: ESP32-S3-N16R8 (16MB Flash + 8MB PSRAM)
**开发框架**: **纯 ESP-IDF (v5.x)** - **全面弃用 Arduino 框架**
**核心功能**: I2S音频 + SPI TFT + OV2640摄像头 + **MQTTS** + **HTTPS** + LVGL UI
**任务调度**: FreeRTOS 原生多任务调度
**通信架构**: **公网加密双通道** (MQTTS轻数据 + HTTPS重数据)

**核心原则**:
1. **禁止造轮子** - 优先复用官方代码并用 ESP-IDF API 重写
2. **工业级稳定** - 使用原生 ESP-IDF API 而非 Arduino 封装
3. **FreeRTOS 管理** - 严格使用任务队列、信号量、互斥锁
4. **公网安全** - 必须启用TLS/SSL加密，防止数据泄露和篡改

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

### 状态机 (基于 FreeRTOS Event Group) - V4.0扩展

```c
// 使用事件组管理状态
EventGroupHandle_t system_event_group;

// V3.2 基础状态
#define STATE_IDLE_BIT       BIT0   // 待机
#define STATE_RECORDING_BIT  BIT1   // 录音中
#define STATE_CAPTURING_BIT  BIT2   // 拍照中
#define STATE_THINKING_BIT   BIT3   // 推理中
#define STATE_PLAYING_BIT    BIT4   // 播放中
#define STATE_COOLDOWN_BIT   BIT5   // 冷却期(3秒)

// V4.0 新增公网状态
#define STATE_TLS_HANDSHAKE_BIT  BIT6   // TLS握手中 (显示"安全连接中...")
#define STATE_CLOUD_SYNC_BIT     BIT7   // 云端同步中 (显示"上行同步中...")
#define STATE_UPLOADING_BIT      BIT8   // 图片上传中 (显示HTTPS上传进度)
```

### 状态监听任务的正确写法 (2026-04-16更新)

**问题**: 使用`xEventGroupWaitBits()`监听状态变化，但UI不更新

**原因**: `xEventGroupWaitBits()`在状态位一直为1时会立即返回，无法检测真正的状态变化

#### Wrong (无法检测状态变化)
```c
void state_monitor_task(void *pvParameters) {
    EventGroupHandle_t event_group = state_machine_get_event_group();

    while (1) {
        // ❌ 如果STATE_IDLE_BIT一直为1，会立即返回，不等待变化
        EventBits_t bits = xEventGroupWaitBits(
            event_group,
            STATE_IDLE_BIT | STATE_RECORDING_BIT | STATE_PLAYING_BIT,
            pdFALSE,  // 不清除位
            pdFALSE,  // 任意位满足即可
            pdMS_TO_TICKS(1000)
        );

        if (bits & STATE_IDLE_BIT) {
            update_ui_idle();  // 只在第一次执行，后续不会触发
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

#### Correct (正确检测状态变化)
```c
void state_monitor_task(void *pvParameters) {
    EventGroupHandle_t event_group = state_machine_get_event_group();
    EventBits_t last_state = 0;

    while (1) {
        // ✅ 直接获取当前状态
        EventBits_t current_state = xEventGroupGetBits(event_group);

        // ✅ 只在状态真正变化时更新UI
        if (current_state != last_state) {
            ESP_LOGI(TAG, "状态变化: 0x%02X -> 0x%02X", last_state, current_state);

            if (current_state & STATE_IDLE_BIT) {
                update_ui_idle();
            } else if (current_state & STATE_RECORDING_BIT) {
                update_ui_recording();
            } else if (current_state & STATE_PLAYING_BIT) {
                update_ui_playing();
            }

            last_state = current_state;
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms检查一次
    }
}
```

**关键点**:
- 使用`xEventGroupGetBits()`直接获取当前状态
- 保存`last_state`进行比较，只在变化时更新UI
- 轮询间隔100ms，平衡响应性和CPU占用

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
static lv_display_t *lvgl_display = NULL;  // 保存display句柄用于强制刷新

// UI 刷新任务
void lvgl_task(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            lv_timer_handler();  // LVGL v9核心刷新
            xSemaphoreGive(lvgl_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(5));  // 200Hz刷新率
    }
}

// 其他任务更新 UI 时也必须加锁
void update_ui_from_other_task() {
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lv_label_set_text(label, "新文本");

        // CRITICAL: LVGL v9必须强制立即刷新!
        lv_obj_invalidate(label);       // 标记对象需要重绘
        lv_refr_now(lvgl_display);      // 立即强制刷新显示器

        xSemaphoreGive(lvgl_mutex);
    }
}
```

### LVGL v9刷新机制 (CRITICAL! - 2026-04-16更新)

**问题**: 调用`lv_label_set_text()`或`lv_obj_set_style_bg_color()`后，屏幕不刷新

**原因**: LVGL v9的刷新是**异步的**
- `lv_obj_invalidate(obj)`: 只是标记对象为"脏"，等待下次刷新周期
- 如果刷新任务延迟或被阻塞，UI更新不会立即显示

**解决方案**: 使用`lv_refr_now()`强制立即刷新

#### Wrong (屏幕不更新)
```c
void set_wifi_status(bool connected) {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    if (connected) {
        lv_label_set_text(wifi_icon, "WiFi:ON");
        lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x00FF00), 0);
    }

    // ❌ 只标记需要重绘，但不会立即刷新
    lv_obj_invalidate(wifi_icon);

    xSemaphoreGive(lvgl_mutex);
}
// 结果: 日志显示已更新，但屏幕还是显示旧内容
```

#### Correct (立即刷新)
```c
void set_wifi_status(bool connected) {
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (connected) {
            lv_label_set_text(wifi_icon, "WiFi:ON");
            lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x00FF00), 0);
        }

        // ✅ 强制立即刷新
        lv_obj_invalidate(wifi_icon);       // 标记需要重绘
        lv_refr_now(lvgl_display);          // 立即刷新显示器 ← 关键!

        xSemaphoreGive(lvgl_mutex);
    }
}
// 结果: 屏幕立即显示新内容
```

**适用场景**:
- 状态更新 (WiFi/MQTT连接状态)
- 用户交互响应 (按钮点击、触摸事件)
- 远程控制指令 (MQTT下发的UI更新)

**性能影响**: `lv_refr_now()`会立即触发完整刷新周期，频繁调用可能影响性能
- ✅ 状态变化时调用 (低频)
- ❌ 不要在动画循环中调用 (高频)

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

### LVGL v9容器样式 (2026-04-16更新)

**问题**: 容器背景色显示异常（如设置#222222灰色，实际显示绿色）

**原因**: LVGL v9容器默认有`padding`，导致背景色未填充整个区域

**解决方案**: 显式清除padding

#### Wrong (背景色异常)
```c
lv_obj_t *status_bar = lv_obj_create(lv_screen_active());
lv_obj_set_size(status_bar, 240, 30);
lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x222222), 0);
// ❌ 缺少padding设置，容器内部留白，背景色不完整
```

#### Correct (背景色正确)
```c
lv_obj_t *status_bar = lv_obj_create(lv_screen_active());
lv_obj_set_size(status_bar, 240, 30);
lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x222222), 0);
lv_obj_set_style_border_width(status_bar, 0, 0);
lv_obj_set_style_radius(status_bar, 0, 0);
lv_obj_set_style_pad_all(status_bar, 0, 0);  // ✅ 清除所有padding
```

**适用场景**:
- 全屏容器 (背景必须填满)
- 状态栏/工具栏 (无边距设计)

**特殊情况**: 字幕区域需要文字边距时，设置合理的padding
```c
lv_obj_set_style_pad_all(subtitle_container, 10, 0);  // 10px padding用于文字边距
```

---

## 公网加密通信 (V4.0 新增)

### 通信架构设计

**双通道策略**: 重数据走HTTPS，轻数据走MQTTS

| 通道类型 | 协议 | 用途 | 数据类型 |
|---------|------|------|---------|
| **MQTTS** | MQTT over TLS | 实时控制流、设备状态流、音频数据 | Base64音频、JSON指令 |
| **HTTPS** | HTTP over SSL | 大体积图片上传 | JPEG图片(<300KB) |

### MQTTS 配置 (必须启用TLS)

**CRITICAL**: 公网部署必须开启TLS/SSL加密，防止数据窃听和篡改

```c
#include "mqtt_client.h"
#include "esp_crt_bundle.h"

// 外部声明嵌入的CA证书
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");
extern const uint8_t server_cert_pem_end[]   asm("_binary_server_cert_pem_end");

esp_mqtt_client_config_t mqtt_cfg = {
    .broker = {
        .address.uri = "mqtts://your-server.com:8883",  // 使用mqtts://和8883端口
        .verification.certificate = (const char *)server_cert_pem_start,  // CA证书
    },
    .credentials = {
        .client_id = "esp32_device_001",
        .username = "your-username",   // EMQX认证
        .password = "your-password",
    },
};

esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
esp_mqtt_client_start(client);

// 订阅主题
esp_mqtt_client_subscribe(client, "product/esp32/001/playaudio", 1);
esp_mqtt_client_subscribe(client, "product/esp32/001/ui", 1);
esp_mqtt_client_subscribe(client, "product/esp32/001/control", 1);
```

**证书烧录配置** (CMakeLists.txt):
```cmake
target_add_binary_data(your_project.elf "certs/server_cert.pem" TEXT)
```

### HTTPS 图片上传 (V4.0 新增)

**用途**: OV2640抓拍后通过HTTPS POST直传云端FastAPI

```c
#include "esp_http_client.h"

esp_err_t upload_jpeg_to_cloud(const uint8_t *jpeg_data, size_t jpeg_len) {
    char url[256];
    snprintf(url, sizeof(url), "https://your-server.com/api/v1/vision/upload");

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .cert_pem = (const char *)server_cert_pem_start,  // SSL证书
        .timeout_ms = 10000,  // 10秒超时
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // 设置鉴权Token
    esp_http_client_set_header(client, "Authorization", "Bearer YOUR_JWT_TOKEN");
    esp_http_client_set_header(client, "Content-Type", "image/jpeg");

    // 上传图片数据
    esp_http_client_set_post_field(client, (const char *)jpeg_data, jpeg_len);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS上传成功, 状态码=%d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTPS上传失败: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
```

### 内存管理优化 (CRITICAL!)

**最大风险**: TLS/SSL握手时mbedTLS需要30-40KB连续内存，如果分配失败会导致设备重启

**必须配置** (sdkconfig):
```ini
# 强制mbedTLS内存分配到PSRAM
CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=16384
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=4096
CONFIG_MBEDTLS_DYNAMIC_BUFFER=y
CONFIG_MBEDTLS_DYNAMIC_FREE_PEER_CERT=y
CONFIG_MBEDTLS_DYNAMIC_FREE_CONFIG_DATA=y

# PSRAM配置
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```

**关键代码** (app_main.c):
```c
void app_main(void) {
    // 启用PSRAM
    esp_err_t ret = esp_psram_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PSRAM初始化失败!");
        return;
    }

    // 检查可用内存
    ESP_LOGI(TAG, "Free heap: %d, PSRAM: %d",
             esp_get_free_heap_size(),
             esp_get_free_internal_heap_size());

    // ... 其他初始化
}
```

## MQTT Topics (公网鉴权分层设计)

```
{product}/esp32/{device_id}/audio       # ESP32 上传录音 (Base64)
{product}/esp32/{device_id}/playaudio   # 云端下发播放音频 (MP3 Base64)
{product}/esp32/{device_id}/ui          # 云端下发 UI 状态 (JSON: text/emotion/status)
{product}/esp32/{device_id}/control     # 远程控制指令 (拍照/静音) - V4.0新增
{product}/esp32/{device_id}/status      # ESP32 上报设备状态 (心跳、错误)
emqx/system/logs                        # 全局日志主题
```

---

## 核心原则

### ✅ DO

- 优先复用官方blog_4+blog_6代码
- 严格遵循状态机,避免状态混乱
- 播放后强制3秒冷却
- MQTT断线自动重连
- **所有代码的注释必须使用中文**
- **每次写完代码后必须使用codex MCP工具进行代码审查** ← CRITICAL!

### ❌ DON'T

- ❌ 不要自己写I2S/摄像头驱动
- ❌ 不要跳过冷却期(会导致回声)
- ❌ 不要在PSRAM<8MB的芯片上跑LVGL全帧缓冲
- ❌ **写完代码之后不需要写README.md等文档**
- ❌ **不要跳过代码审查直接提交代码**

---

## 代码审查规范 (MANDATORY)

**每次编写/修改代码后，必须按以下顺序进行审查**:

### 1. 自测 (开发者自己完成)
```bash
# ESP-IDF项目编译测试
idf.py build

# 检查编译警告
idf.py build 2>&1 | grep "warning:"

# 如果有条件，烧录到设备进行功能测试
idf.py flash monitor
```

### 2. codex MCP代码审查 (CRITICAL!)

**在提交代码前，必须调用codex MCP工具进行全面审查**

codex MCP会自动检查:
- [ ] 是否复用了现有官方代码/库 (禁止造轮子)
- [ ] FreeRTOS任务/互斥锁使用是否正确
- [ ] 内存分配是否使用PSRAM
- [ ] LVGL UI操作是否加锁
- [ ] 状态机状态转换是否合理
- [ ] 代码注释是否使用中文
- [ ] 配置外部化 (不硬编码参数)
- [ ] TLS/SSL配置是否安全

**使用方法**: 在代码编写完成后，调用MCP工具

### 3. 审查通过后才能提交

```bash
# 只有在codex MCP审查通过后才能提交
git add <修改的文件>
git commit -m "feat(scope): 描述"
```

---

**总结**: 复用官方代码 + 状态机管理 + LVGL UI + 性能优化 + **代码审查**

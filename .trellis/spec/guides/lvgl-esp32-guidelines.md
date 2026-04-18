# LVGL v9 + ESP32-S3 开发规范

> ESP32-S3硬件 + LVGL v9图形库开发的关键知识和常见陷阱
>
> **版本**: V1.0 (2026-04-16创建)

---

## Overview

**硬件**: ESP32-S3 + ILI9341 SPI TFT屏幕 (240x320)
**LVGL版本**: v9.x
**驱动**: ESP-IDF原生`esp_lcd`组件
**核心挑战**: 线程安全 + 强制刷新 + 容器样式

---

## 核心原则

1. **强制刷新必不可少** - `lv_refr_now()`是LVGL v9在多任务环境中的关键
2. **容器padding必须显式控制** - LVGL v9默认padding会导致背景色异常
3. **线程安全靠Mutex** - 所有UI操作必须加锁
4. **状态监听用轮询** - `xEventGroupGetBits()`比`xEventGroupWaitBits()`更可靠

---

## 1. LVGL v9刷新机制 (CRITICAL!)

### 问题: UI更新不显示

**症状**:
```
I (xxx) LVGL_UI: WiFi状态已更新: ON (绿色)  ← 日志显示已更新
I (xxx) LVGL_UI: 字幕已更新: Listening...

但TFT屏幕还是显示旧内容 ❌
```

**根本原因**: LVGL v9刷新是异步的
- `lv_label_set_text()`: 只改变数据结构
- `lv_obj_invalidate()`: 只标记"需要重绘"，不会立即刷新
- `lv_timer_handler()`: 定时器触发时才真正刷新，可能延迟或被阻塞

### 解决方案: 强制立即刷新

#### Signatures
```c
// LVGL v9 API
void lv_obj_invalidate(lv_obj_t *obj);              // 标记对象为脏
void lv_refr_now(lv_display_t *disp);               // 立即强制刷新显示器
uint32_t lv_timer_handler(void);                    // 处理LVGL定时器
```

#### Contracts

**WiFi/MQTT状态更新**:
```c
void lvgl_ui_set_wifi_status(bool connected);
void lvgl_ui_set_mqtt_status(bool connected);
```

**Request**:
- `connected`: true=已连接, false=断开

**Side Effects**:
- 修改label文本 ("WiFi:ON" / "WiFi:OFF")
- 修改文本颜色 (绿色0x00FF00 / 红色0xFF0000)
- **立即刷新屏幕** (调用`lv_refr_now()`)

#### Validation & Error Matrix

| Condition | Behavior |
|-----------|----------|
| `wifi_icon == NULL` | 打印警告日志，跳过更新 |
| 获取互斥锁超时 (>100ms) | 打印错误日志，放弃更新 |
| 正常情况 | 更新UI + 强制刷新 |

#### Good/Base/Bad Cases

**Good Case** (立即刷新):
```c
void set_wifi_status(bool connected) {
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (connected) {
            lv_label_set_text(wifi_icon, "WiFi:ON");
            lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x00FF00), 0);
        }

        lv_obj_invalidate(wifi_icon);       // 标记重绘
        lv_refr_now(lvgl_display);          // 立即刷新 ← 关键!

        xSemaphoreGive(lvgl_mutex);
    }
}
```

**Bad Case** (屏幕不刷新):
```c
void set_wifi_status(bool connected) {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    if (connected) {
        lv_label_set_text(wifi_icon, "WiFi:ON");
        lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x00FF00), 0);
    }

    lv_obj_invalidate(wifi_icon);  // ❌ 只标记，不立即刷新

    xSemaphoreGive(lvgl_mutex);
}
// 结果: 日志显示已更新，但屏幕还是旧内容
```

**Base Case** (不使用强制刷新):
```c
// 依赖lv_timer_handler()自动刷新
// 优点: 性能更好 (批量刷新)
// 缺点: 延迟高 (可能几百毫秒才刷新)
```

#### Tests Required

**单元测试** (模拟):
```c
void test_wifi_status_update() {
    // 模拟WiFi连接
    lvgl_ui_set_wifi_status(true);

    // 断言: label文本已改变
    assert(strcmp(lv_label_get_text(wifi_icon), "WiFi:ON") == 0);

    // 断言: 颜色已改变
    lv_color_t color = lv_obj_get_style_text_color(wifi_icon, 0);
    assert(color.full == lv_color_hex(0x00FF00).full);
}
```

**集成测试** (硬件):
```c
void test_screen_refresh() {
    // 1. 初始状态: WiFi:OFF (红色)
    assert_screen_shows("WiFi:OFF", RED);

    // 2. 更新状态
    lvgl_ui_set_wifi_status(true);

    // 3. 等待刷新 (应该立即刷新，不超过100ms)
    vTaskDelay(pdMS_TO_TICKS(100));

    // 4. 验证屏幕显示
    assert_screen_shows("WiFi:ON", GREEN);
}
```

#### Wrong vs Correct

##### Wrong (不立即刷新)
```c
void lvgl_ui_set_emotion(emotion_t emotion) {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    switch (emotion) {
        case EMOTION_HAPPY:
            lv_obj_set_style_bg_color(emotion_obj, lv_color_hex(0xFFD700), 0);
            break;
    }

    lv_obj_invalidate(emotion_obj);  // ❌ 只标记

    xSemaphoreGive(lvgl_mutex);
}
// 问题: 状态机切换到RECORDING，但屏幕圆形还是白色
```

##### Correct (立即刷新)
```c
void lvgl_ui_set_emotion(emotion_t emotion) {
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        switch (emotion) {
            case EMOTION_HAPPY:
                lv_obj_set_style_bg_color(emotion_obj, lv_color_hex(0xFFD700), 0);
                ESP_LOGI(TAG, "情绪已更新: HAPPY (金色)");
                break;
        }

        lv_obj_invalidate(emotion_obj);
        lv_refr_now(lvgl_display);  // ✅ 立即刷新

        xSemaphoreGive(lvgl_mutex);
    }
}
// 效果: 状态机切换后，屏幕立即变金色
```

### 性能考虑

**`lv_refr_now()`的成本**:
- 触发完整刷新周期，遍历所有脏对象
- ILI9341 SPI传输 (40MHz，240x320像素)
- 典型刷新时间: 10-30ms

**使用建议**:
- ✅ 低频状态更新 (WiFi连接、MQTT连接、状态机切换)
- ❌ 高频动画循环 (帧率>30FPS时依赖自动刷新)

---

## 2. LVGL v9容器样式陷阱

### 问题: 背景色显示异常

**症状**:
```c
lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x222222), 0);  // 设置灰色
// 实际屏幕显示: 绿色 ❌
```

**根本原因**: LVGL v9容器默认有padding，背景色未填充整个区域

### 解决方案: 显式清除padding

#### Signatures
```c
void lv_obj_set_style_pad_all(lv_obj_t *obj, int32_t value, lv_style_selector_t selector);
void lv_obj_set_style_pad_top(lv_obj_t *obj, int32_t value, lv_style_selector_t selector);
void lv_obj_set_style_pad_bottom(lv_obj_t *obj, int32_t value, lv_style_selector_t selector);
void lv_obj_set_style_pad_left(lv_obj_t *obj, int32_t value, lv_style_selector_t selector);
void lv_obj_set_style_pad_right(lv_obj_t *obj, int32_t value, lv_style_selector_t selector);
```

#### Contracts

**状态栏容器创建**:
```c
lv_obj_t* create_status_bar(void);
```

**Postconditions**:
- 容器尺寸: 240x30 (全屏宽度)
- 背景色: #222222 (深灰色)
- **所有padding为0** (背景填满整个容器)
- border_width=0, radius=0 (无边框、无圆角)

#### Validation & Error Matrix

| Condition | Result |
|-----------|--------|
| 未设置`pad_all` | 背景色不填满，显示底层颜色 |
| 设置`pad_all=0` | 背景色完全填满 ✅ |
| 设置`pad_all=10` | 背景色四周留10px空白 |

#### Good/Base/Bad Cases

**Good Case** (背景色正确):
```c
lv_obj_t *status_bar = lv_obj_create(lv_screen_active());
lv_obj_set_size(status_bar, TFT_H_RES, 30);
lv_obj_set_pos(status_bar, 0, 0);
lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x222222), 0);
lv_obj_set_style_border_width(status_bar, 0, 0);
lv_obj_set_style_radius(status_bar, 0, 0);
lv_obj_set_style_pad_all(status_bar, 0, 0);  // ✅ 清除padding
```

**Bad Case** (背景色异常):
```c
lv_obj_t *status_bar = lv_obj_create(lv_screen_active());
lv_obj_set_size(status_bar, TFT_H_RES, 30);
lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x222222), 0);
// ❌ 忘记清除padding
// 结果: 背景色不完整，显示底层颜色 (可能是绿色默认背景)
```

**Base Case** (字幕区需要padding):
```c
lv_obj_t *subtitle_container = lv_obj_create(lv_screen_active());
lv_obj_set_size(subtitle_container, TFT_H_RES, 80);
lv_obj_set_style_bg_color(subtitle_container, lv_color_hex(0x222222), 0);
lv_obj_set_style_pad_all(subtitle_container, 10, 0);  // ✅ 10px padding用于文字边距
```

#### Wrong vs Correct

##### Wrong (忘记清除padding)
```c
void create_status_bar(void) {
    lv_obj_t *status_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(status_bar, 240, 30);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x222222), 0);
    // ❌ 缺少padding设置
}
// 问题: 背景色显示为绿色而非灰色
```

##### Correct (显式清除padding)
```c
void create_status_bar(void) {
    lv_obj_t *status_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(status_bar, 240, 30);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);  // ✅ 清除padding
}
// 效果: 背景色完全填充，显示正确的灰色
```

---

## 3. FreeRTOS状态监听任务

### 问题: 状态变化检测失效

**症状**:
```c
// 状态机: IDLE -> RECORDING
// 预期: UI立即更新为金色圆形 + "Recording..."
// 实际: UI不变化 ❌
```

**根本原因**: `xEventGroupWaitBits()`在状态位一直为1时立即返回

#### Wrong (无法检测变化)
```c
void state_monitor_task(void *pvParameters) {
    EventGroupHandle_t event_group = state_machine_get_event_group();

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            event_group,
            STATE_IDLE_BIT | STATE_RECORDING_BIT,
            pdFALSE,  // 不清除位
            pdFALSE,  // 任意位满足
            pdMS_TO_TICKS(1000)
        );

        if (bits & STATE_IDLE_BIT) {
            update_ui_idle();  // 只执行一次，后续不触发
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
// 问题: IDLE状态一直为1，每次都立即返回，无法检测切换到RECORDING
```

#### Correct (轮询+状态比较)
```c
void state_monitor_task(void *pvParameters) {
    EventGroupHandle_t event_group = state_machine_get_event_group();
    EventBits_t last_state = 0;

    while (1) {
        EventBits_t current_state = xEventGroupGetBits(event_group);

        if (current_state != last_state) {
            ESP_LOGI(TAG, "状态变化: 0x%02X -> 0x%02X", last_state, current_state);

            if (current_state & STATE_IDLE_BIT) {
                update_ui_idle();
            } else if (current_state & STATE_RECORDING_BIT) {
                update_ui_recording();
            }

            last_state = current_state;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
// 效果: 每100ms检查一次，只在真正变化时更新UI
```

---

## 4. 线程安全最佳实践

### Mutex超时设置

#### Wrong (无限等待 - 可能死锁)
```c
void update_ui() {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);  // ❌ 可能永久阻塞
    lv_label_set_text(label, "text");
    xSemaphoreGive(lvgl_mutex);
}
```

#### Correct (带超时 - 优雅降级)
```c
void update_ui() {
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lv_label_set_text(label, "text");
        lv_refr_now(lvgl_display);
        xSemaphoreGive(lvgl_mutex);
    } else {
        ESP_LOGE(TAG, "获取LVGL互斥锁失败!");  // 记录错误，继续运行
    }
}
```

---

## 5. 调试技巧

### 添加调试日志

**必须日志**:
```c
ESP_LOGI(TAG, "WiFi状态已更新: ON (绿色)");
ESP_LOGI(TAG, "状态变化检测: 0x%02X -> 0x%02X", last_state, current_state);
```

**日志验证清单**:
- [ ] UI函数是否被调用？
- [ ] 互斥锁是否成功获取？
- [ ] `lv_refr_now()`是否被执行？
- [ ] 状态监听任务是否检测到变化？

---

## 6. 完整初始化模板

```c
// 全局变量
static lv_display_t *lvgl_display = NULL;
static SemaphoreHandle_t lvgl_mutex = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *emotion_obj = NULL;

// 初始化流程
esp_err_t lvgl_ui_init(esp_lcd_panel_handle_t panel_handle) {
    // 1. 创建互斥锁
    lvgl_mutex = xSemaphoreCreateMutex();

    // 2. 初始化LVGL
    lv_init();

    // 3. 分配显示缓冲区到PSRAM
    size_t buffer_size = TFT_H_RES * 32 * sizeof(lv_color_t);
    lv_color_t *buf1 = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);

    // 4. 创建显示设备
    lvgl_display = lv_display_create(TFT_H_RES, TFT_V_RES);
    lv_display_set_flush_cb(lvgl_display, lvgl_flush_cb);
    lv_display_set_buffers(lvgl_display, buf1, buf2, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(lvgl_display, panel_handle);

    // 5. 创建UI组件
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    create_status_bar();
    create_emotion_display();
    create_subtitle_area();
    xSemaphoreGive(lvgl_mutex);

    // 6. 创建LVGL刷新任务
    xTaskCreate(lvgl_task, "LVGL_Task", 8192, NULL, 2, NULL);

    // 7. 创建状态监听任务
    xTaskCreate(state_monitor_task, "State_Monitor", 4096, NULL, 2, NULL);

    return ESP_OK;
}
```

---

---

## 7. SquareLine Studio 集成规范 (V1.1 新增)

### SquareLine Studio 工作流

**工具用途**: 使用SquareLine Studio可视化设计UI，导出LVGL C代码

**导出文件结构**:
```
squareline_export/
├── ui/
│   ├── ui.h            # UI组件声明（全局变量）
│   ├── ui.c            # UI初始化和创建函数
│   ├── screens/        # 屏幕定义
│   └── components/     # 自定义组件
```

### UI组件全局变量映射 (基于UI_prd.md)

**SquareLine Studio导出的核心变量** (在`ui.h`中声明):

```c
// 状态栏图标
extern lv_obj_t *ui_label_wifi11;   // WiFi连接图标
extern lv_obj_t *ui_label_mqtt11;   // MQTT/云端同步图标

// 情绪核心组件
extern lv_obj_t *ui_EmotionCircle11;    // 底部圆形面板（情绪背景）
extern lv_obj_t *ui_uiEmotionLabel11;   // 圆圈内部文本标签（表情符号）

// 动画函数
lv_anim_t * breathe_Animation(lv_obj_t *TargetObject, int delay);
```

**注意**: SquareLine Studio生成的变量名带数字后缀（如`ui_label_wifi11`），请以实际生成的`screens/ui_MainScreen.h`为准

### Contracts (UI更新接口规范)

#### 网络状态更新

**Signature**:
```c
void update_network_ui(bool wifi_ok, bool mqtt_ok);
```

**Request**:
- `wifi_ok`: WiFi连接状态 (true=已连接, false=断开)
- `mqtt_ok`: MQTT连接状态 (true=已连接, false=断开)

**Side Effects**:
- 修改`ui_label_wifi1`文本颜色 (绿色=正常, 灰色=异常)
- 修改`ui_label_mqtt1`文本颜色 (蓝色=正常, 灰色=异常)
- 立即刷新屏幕 (调用`lv_refr_now()`)

**Implementation**:
```c
void update_network_ui(bool wifi_ok, bool mqtt_ok) {
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

        lv_obj_invalidate(ui_label_wifi1);
        lv_obj_invalidate(ui_label_mqtt1);
        lv_refr_now(lvgl_display);

        xSemaphoreGive(lvgl_mutex);
    }
}
```

#### 情绪状态切换

**Signature**:
```c
void change_emotion_state(int state);
```

**Request**:
- `state`: 状态机状态常量
  - `STATE_LISTENING`: 倾听/待机模式
  - `STATE_SPEAKING`: 说话/思考模式

**Side Effects**:
- **STATE_LISTENING**:
  - 停止所有动画 (`lv_anim_del()`)
  - 固定尺寸为120x120
  - 背景色设为浅灰色(0xD3D3D3)
  - 内部显示"..."文本
- **STATE_SPEAKING**:
  - 清空内部文本
  - 背景色设为活力绿色(0x32CD32)
  - 启动呼吸动画 (`breathe_Animation()`)

**Implementation**:
```c
void change_emotion_state(int state) {
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (state == STATE_LISTENING) {
            // 停止所有动画
            lv_anim_del(ui_EmotionCircle1, NULL);

            // 固定尺寸120x120
            lv_obj_set_width(ui_EmotionCircle1, 120);
            lv_obj_set_height(ui_EmotionCircle1, 120);

            // 设置浅灰色背景
            lv_obj_set_style_bg_color(
                ui_EmotionCircle1,
                lv_color_hex(0xD3D3D3),
                0
            );

            // 显示倾听表情
            lv_label_set_text(ui_uiEmotionLabel1, "...");

            ESP_LOGI(TAG, "UI状态: 倾听模式（静态圆形）");
        }
        else if (state == STATE_SPEAKING) {
            // 清空表情文本
            lv_label_set_text(ui_uiEmotionLabel1, "");

            // 设置活力绿色
            lv_obj_set_style_bg_color(
                ui_EmotionCircle1,
                lv_color_hex(0x32CD32),
                0
            );

            // 启动呼吸动画
            breathe_Animation(ui_EmotionCircle1, 0);

            ESP_LOGI(TAG, "UI状态: 说话模式（呼吸动画）");
        }

        lv_obj_invalidate(ui_EmotionCircle1);
        lv_obj_invalidate(ui_uiEmotionLabel1);
        lv_refr_now(lvgl_display);

        xSemaphoreGive(lvgl_mutex);
    }
}
```

### Validation & Error Matrix

| Condition | Behavior |
|-----------|----------|
| `ui_EmotionCircle1 == NULL` | 打印错误日志，跳过更新 |
| `ui_uiEmotionLabel1 == NULL` | 打印警告日志，跳过文本更新 |
| 获取互斥锁超时 (>100ms) | 打印错误日志，放弃更新 |
| 正常情况 | 更新UI + 强制刷新 |

### Good/Base/Bad Cases

**Good Case** (完整实现):
```c
void change_emotion_state(int state) {
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (state == STATE_LISTENING) {
            lv_anim_del(ui_EmotionCircle1, NULL);
            lv_obj_set_size(ui_EmotionCircle1, 120, 120);
            lv_obj_set_style_bg_color(ui_EmotionCircle1, lv_color_hex(0xD3D3D3), 0);
            lv_label_set_text(ui_uiEmotionLabel1, "...");
        }

        lv_obj_invalidate(ui_EmotionCircle1);
        lv_refr_now(lvgl_display);  // ✅ 强制刷新

        xSemaphoreGive(lvgl_mutex);
    }
}
```

**Bad Case** (忘记停止动画):
```c
void change_emotion_state(int state) {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    if (state == STATE_LISTENING) {
        // ❌ 忘记停止动画，导致圆形继续呼吸
        lv_obj_set_size(ui_EmotionCircle1, 120, 120);
        lv_obj_set_style_bg_color(ui_EmotionCircle1, lv_color_hex(0xD3D3D3), 0);
    }

    xSemaphoreGive(lvgl_mutex);
}
// 问题: 倾听模式下圆形还在呼吸，不符合设计
```

**Base Case** (不使用SquareLine变量):
```c
// 手动创建UI组件，不使用SquareLine Studio生成的变量
lv_obj_t *emotion_circle = lv_obj_create(lv_screen_active());
// ...手动配置样式和动画
// 缺点: 失去可视化编辑的便利性
```

### 集成步骤

**1. 导入SquareLine Studio导出的代码**:
```bash
# 复制导出的ui/目录到ESP-IDF项目的main/目录
cp -r squareline_export/ui/ esp32-s3-mqtts-audio/main/
```

**2. 在CMakeLists.txt中添加ui源文件**:
```cmake
idf_component_register(
    SRCS
        "main.c"
        "lvgl_ui.c"
        "ui/ui.c"              # SquareLine生成的UI代码
        "ui/screens/ui_Screen1.c"  # 屏幕定义
    INCLUDE_DIRS "." "ui"
)
```

**3. 在lvgl_ui.c中引入ui.h**:
```c
#include "ui.h"

// 在LVGL初始化后调用
void lvgl_ui_init(void) {
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    ui_init();  // 渲染所有SquareLine生成的UI组件
    xSemaphoreGive(lvgl_mutex);
}
```

**4. 在状态监听任务中调用UI更新函数**:
```c
void state_monitor_task(void *pvParameters) {
    EventGroupHandle_t event_group = state_machine_get_event_group();
    EventBits_t last_state = 0;

    while (1) {
        EventBits_t current_state = xEventGroupGetBits(event_group);

        if (current_state != last_state) {
            if (current_state & STATE_IDLE_BIT) {
                change_emotion_state(STATE_LISTENING);  // ✅ 调用UI更新
            } else if (current_state & STATE_RECORDING_BIT) {
                change_emotion_state(STATE_SPEAKING);
            }

            last_state = current_state;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 注意事项

1. **SquareLine Studio版本**: 建议使用v1.3+，支持LVGL v9导出
2. **动画函数**: `breathe_Animation()`是自动生成的，参数delay通常传0
3. **组件命名**: SquareLine中的对象名会转换为C变量名（如`EmotionCircle` → `ui_EmotionCircle1`）
4. **样式覆盖**: 在代码中修改的样式会覆盖SquareLine中设置的默认样式

---

## 总结

| 陷阱 | 表现 | 解决方案 |
|------|------|---------|
| LVGL v9刷新延迟 | 日志已更新，屏幕不变 | `lv_refr_now(lvgl_display)` |
| 容器默认padding | 背景色显示异常 | `lv_obj_set_style_pad_all(obj, 0, 0)` |
| EventGroup等待逻辑 | 状态变化检测失效 | `xEventGroupGetBits()` + 轮询 |
| Mutex无限等待 | 可能死锁 | 设置超时 `pdMS_TO_TICKS(100)` |
| SquareLine动画未停止 | 倾听模式下仍呼吸 | `lv_anim_del(ui_EmotionCircle1, NULL)` |

**核心口诀**: 改完UI必刷新，容器padding要清零，状态监听用轮询，加锁超时防死锁，切换状态先停动画！

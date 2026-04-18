# SquareLine Studio 集成规范 (ESP32-S3 + LVGL v9)

> 基于SquareLine Studio 1.6.0生成的LVGL v9.2.2代码集成到ESP-IDF项目的完整指南
>
> **版本**: V1.0 (2026-04-18创建)
> **基于实际生成代码**: `/home/xiapiya/AI_FLY/esp32/esp32-s3-mqtts-audio/main/AIUI/`

---

## Overview

**工具**: SquareLine Studio 1.6.0
**LVGL版本**: 9.2.2
**目标平台**: ESP32-S3 + ESP-IDF
**用途**: 可视化设计UI，导出C代码，集成到嵌入式项目

**核心优势**:
- 可视化编辑UI组件和布局
- 自动生成LVGL v9代码
- 支持动画、样式、事件绑定
- 减少手写UI代码的工作量

---

## 1. 生成的文件结构

### SquareLine Studio导出的文件组织

```
AIUI/                           # SquareLine Studio导出目录
├── ui.h                        # 主头文件（包含所有UI变量声明）
├── ui.c                        # 主实现文件（ui_init()、动画定义）
├── ui_events.h                 # 事件处理声明
├── ui_helpers.h                # 辅助函数声明
├── ui_helpers.c                # 辅助函数实现（动画回调等）
├── screens/                    # 屏幕定义
│   ├── ui_MainScreen.h        # 主屏幕变量声明
│   └── ui_MainScreen.c        # 主屏幕初始化代码
├── components/                 # 自定义组件
│   └── ui_comp_hook.c         # 组件回调（本项目为空）
├── fonts/                      # 字体文件（如有）
├── CMakeLists.txt             # ESP-IDF构建配置
├── filelist.txt               # 文件列表
└── project.info               # 项目元数据（JSON格式）
```

### 关键文件说明

| 文件 | 用途 | 是否可修改 |
|------|------|----------|
| `ui.h` | 全局UI变量声明、函数原型 | ❌ 重新导出会覆盖 |
| `ui.c` | `ui_init()`入口、动画定义 | ❌ 重新导出会覆盖 |
| `screens/ui_MainScreen.c` | 屏幕组件创建代码 | ❌ 重新导出会覆盖 |
| `ui_events.h` | 事件处理声明 | ✅ 可手动添加事件处理函数 |
| `CMakeLists.txt` | ESP-IDF构建脚本 | ✅ 可根据项目调整 |

**重要**: SquareLine Studio重新导出时会**覆盖所有自动生成的文件**，因此不要直接修改这些文件！

---

## 2. UI组件变量映射 (基于实际生成代码)

### 全局变量命名规则

**SquareLine Studio生成的变量名规则**:
```
ui_<ComponentName><InstanceNumber>
```

**示例**: 如果在SquareLine中创建了一个名为`EmotionCircle`的对象，生成的C变量名为`ui_EmotionCircle1`

### 本项目的UI组件变量 (MainScreen)

在`screens/ui_MainScreen.h`中声明的全局变量:

```c
// 屏幕对象
extern lv_obj_t * ui_MainScreen;         // 主屏幕容器

// 状态栏组件
extern lv_obj_t * ui_Status_Bar1;        // 状态栏背景容器
extern lv_obj_t * ui_label_wifi1;        // WiFi图标标签 ("[  ]")
extern lv_obj_t * ui_label_mqtt1;        // MQTT图标标签 ("[  ]")

// 情绪显示组件
extern lv_obj_t * ui_EmotionCircle1;     // 底部圆形面板（情绪背景）
extern lv_obj_t * ui_uiEmotionLabel1;    // 圆圈内部文本标签（表情符号）
```

**注意变量名后缀**:
- SquareLine生成的变量都带`1`后缀（如`ui_label_wifi1`）
- 如果在SquareLine中创建多个同名对象，后缀会递增（`ui_Button1`, `ui_Button2`）

### 动画函数

在`ui.c`中定义的动画函数:

```c
// 呼吸动画函数（宽度+高度同时缩放）
lv_anim_t * breathe_Animation(lv_obj_t *TargetObject, int delay);
```

**参数**:
- `TargetObject`: 要应用动画的对象（如`ui_EmotionCircle1`）
- `delay`: 延迟时间（毫秒），通常传0立即启动

**动画效果**:
- 1000ms内宽度/高度增加20px
- 1000ms回弹到原始尺寸
- 无限循环 (`LV_ANIM_REPEAT_INFINITE`)
- 缓入缓出曲线 (`lv_anim_path_ease_in_out`)

---

## 3. ESP-IDF项目集成步骤

### Step 1: 复制生成的代码到项目

```bash
# 假设SquareLine Studio导出目录为 ~/Downloads/SquareLine_Project/
# ESP-IDF项目main目录为 esp32-s3-mqtts-audio/main/

cp -r ~/Downloads/SquareLine_Project/ esp32-s3-mqtts-audio/main/AIUI/
```

### Step 2: 配置CMakeLists.txt

**方法A**: 修改AIUI/CMakeLists.txt（推荐）

SquareLine生成的`AIUI/CMakeLists.txt`:
```cmake
set(SOURCES
    ui.c
    ui_helpers.c
    components/ui_comp_hook.c
    screens/ui_MainScreen.c
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "."
    REQUIRES lvgl
)
```

这样可以让AIUI作为独立组件。

**方法B**: 在main/CMakeLists.txt中包含AIUI源文件

```cmake
idf_component_register(
    SRCS
        "main.c"
        "lvgl_ui.c"
        "AIUI/ui.c"
        "AIUI/ui_helpers.c"
        "AIUI/components/ui_comp_hook.c"
        "AIUI/screens/ui_MainScreen.c"
    INCLUDE_DIRS "." "AIUI"
    REQUIRES lvgl esp_lcd
)
```

### Step 3: 在代码中引入ui.h

在`lvgl_ui.c`中引入SquareLine生成的UI:

```c
// lvgl_ui.c
#include "lvgl/lvgl.h"
#include "AIUI/ui.h"           // SquareLine生成的UI头文件
#include "AIUI/screens/ui_MainScreen.h"  // 屏幕变量声明

// 全局变量（定义在其他地方）
extern SemaphoreHandle_t lvgl_mutex;
extern lv_display_t *lvgl_display;

// LVGL UI初始化函数
esp_err_t lvgl_ui_init(void) {
    // 必须在LVGL初始化后调用
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ui_init();  // SquareLine生成的初始化函数
        xSemaphoreGive(lvgl_mutex);
        ESP_LOGI(TAG, "SquareLine UI initialized");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to take LVGL mutex");
        return ESP_FAIL;
    }
}
```

### Step 4: 验证编译

```bash
cd esp32-s3-mqtts-audio
idf.py build
```

**常见编译错误**:

| 错误 | 原因 | 解决方案 |
|------|------|---------|
| `ui.h: No such file` | INCLUDE_DIRS未配置 | 添加`"AIUI"`到INCLUDE_DIRS |
| `undefined reference to ui_init` | 源文件未添加 | 在CMakeLists.txt中添加`AIUI/ui.c` |
| `LV_COLOR_DEPTH mismatch` | LVGL配置不匹配 | 检查lv_conf.h中`LV_COLOR_DEPTH`是否为16 |

---

## 4. UI更新函数实现 (基于实际变量名)

### 网络状态更新

```c
// lvgl_ui.c

/**
 * 更新WiFi和MQTT连接状态图标
 *
 * @param wifi_ok WiFi连接状态 (true=已连接, false=断开)
 * @param mqtt_ok MQTT连接状态 (true=已连接, false=断开)
 */
void lvgl_ui_update_network_status(bool wifi_ok, bool mqtt_ok) {
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // WiFi状态: 绿色=正常, 灰色=异常
        lv_obj_set_style_text_color(
            ui_label_wifi1,  // ← 注意变量名带"1"后缀
            wifi_ok ? lv_color_hex(0x00FF00) : lv_color_hex(0x808080),
            0
        );

        // MQTT状态: 蓝色=正常, 灰色=异常
        lv_obj_set_style_text_color(
            ui_label_mqtt1,  // ← 注意变量名带"1"后缀
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
```

### 情绪状态切换

```c
// lvgl_ui.c

// 状态常量定义（与状态机同步）
typedef enum {
    UI_STATE_LISTENING = 0,  // 倾听模式：静态圆形 + 灰色 + "..."
    UI_STATE_SPEAKING  = 1,  // 说话模式：呼吸动画 + 绿色
    UI_STATE_THINKING  = 2,  // 思考模式：呼吸动画 + 金色
    UI_STATE_CAPTURING = 3,  // 拍照模式：呼吸动画 + 蓝色
} ui_emotion_state_t;

/**
 * 切换情绪状态UI
 *
 * @param state UI状态（倾听/说话/思考/拍照）
 */
void lvgl_ui_change_emotion_state(ui_emotion_state_t state) {
    if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // CRITICAL: 先停止所有动画，再启动新动画
        lv_anim_del(ui_EmotionCircle1, NULL);  // ← 注意变量名带"1"后缀

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
                breathe_Animation(ui_EmotionCircle1, 0);  // ← 启动动画
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
```

---

## 5. 状态机与UI联动

### 在状态监听任务中调用UI更新

```c
// state_monitor_task.c

void state_monitor_task(void *pvParameters) {
    EventGroupHandle_t event_group = state_machine_get_event_group();
    EventBits_t last_state = 0;

    while (1) {
        EventBits_t current_state = xEventGroupGetBits(event_group);

        // 只在状态真正变化时更新UI
        if (current_state != last_state) {
            ESP_LOGI(TAG, "状态变化: 0x%02X -> 0x%02X", last_state, current_state);

            // 根据状态机状态更新UI
            if (current_state & STATE_IDLE_BIT) {
                lvgl_ui_change_emotion_state(UI_STATE_LISTENING);
            }
            else if (current_state & (STATE_RECORDING_BIT | STATE_PLAYING_BIT)) {
                lvgl_ui_change_emotion_state(UI_STATE_SPEAKING);
            }
            else if (current_state & STATE_THINKING_BIT) {
                lvgl_ui_change_emotion_state(UI_STATE_THINKING);
            }
            else if (current_state & STATE_CAPTURING_BIT) {
                lvgl_ui_change_emotion_state(UI_STATE_CAPTURING);
            }

            last_state = current_state;
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms轮询间隔
    }
}
```

---

## 6. 常见陷阱与解决方案

### 陷阱1: 变量名不匹配

**问题**: UI_prd.md文档中的变量名和实际生成代码不一致

**原因**: SquareLine Studio会自动添加实例编号后缀

**解决方案**:
- ✅ **始终以实际生成的代码为准**（查看`screens/ui_MainScreen.h`）
- ✅ 使用IDE的自动补全避免手写错误
- ❌ 不要假设变量名，一定要检查生成的代码

### 陷阱2: 忘记停止动画

**问题**: 从"说话模式"切换到"倾听模式"时，圆形还在呼吸

**原因**: 没有调用`lv_anim_del()`停止旧动画

**解决方案**:
```c
// ✅ Correct
lv_anim_del(ui_EmotionCircle1, NULL);  // 先停止
lv_obj_set_size(ui_EmotionCircle1, 120, 120);  // 再设置固定尺寸

// ❌ Wrong
lv_obj_set_size(ui_EmotionCircle1, 120, 120);  // 动画还在运行，尺寸会被覆盖
```

### 陷阱3: 重新导出覆盖代码

**问题**: 在SquareLine Studio中修改UI后重新导出，之前手写的代码被覆盖

**解决方案**:
- ✅ 不要直接修改`ui.c`、`ui_MainScreen.c`等自动生成的文件
- ✅ 所有业务逻辑放在独立的`lvgl_ui.c`中
- ✅ 通过全局变量（如`ui_EmotionCircle1`）操作UI组件

### 陷阱4: UI更新不刷新

**问题**: 调用了`lv_label_set_text()`，但屏幕不更新

**解决方案**: 调用`lv_refr_now()`强制刷新
```c
lv_label_set_text(ui_uiEmotionLabel1, "新文本");
lv_obj_invalidate(ui_uiEmotionLabel1);
lv_refr_now(lvgl_display);  // ← 强制刷新
```

详见: [LVGL ESP32 Guidelines](./lvgl-esp32-guidelines.md#1-lvgl-v9刷新机制-critical)

---

## 7. 调试技巧

### 验证组件是否存在

```c
if (ui_EmotionCircle1 == NULL) {
    ESP_LOGE(TAG, "ui_EmotionCircle1未初始化！检查ui_init()是否被调用");
    return;
}
```

### 打印当前颜色

```c
lv_color_t color = lv_obj_get_style_bg_color(ui_EmotionCircle1, 0);
ESP_LOGI(TAG, "当前背景色: 0x%06X", lv_color_to_hex(color));
```

### 监控动画状态

```c
// 检查是否有动画正在运行
if (lv_anim_get(ui_EmotionCircle1, NULL)) {
    ESP_LOGI(TAG, "ui_EmotionCircle1有动画正在运行");
} else {
    ESP_LOGI(TAG, "ui_EmotionCircle1无动画");
}
```

---

## 8. 完整示例代码

### lvgl_ui.h

```c
#ifndef LVGL_UI_H
#define LVGL_UI_H

#include "esp_err.h"
#include <stdbool.h>

// 初始化SquareLine生成的UI
esp_err_t lvgl_ui_init(void);

// 更新网络状态图标
void lvgl_ui_update_network_status(bool wifi_ok, bool mqtt_ok);

// UI状态枚举
typedef enum {
    UI_STATE_LISTENING = 0,
    UI_STATE_SPEAKING  = 1,
    UI_STATE_THINKING  = 2,
    UI_STATE_CAPTURING = 3,
} ui_emotion_state_t;

// 切换情绪状态
void lvgl_ui_change_emotion_state(ui_emotion_state_t state);

#endif // LVGL_UI_H
```

---

## 9. 最佳实践总结

### ✅ DO

1. **始终查看生成的代码** - 不要假设变量名，一定要检查`screens/ui_MainScreen.h`
2. **业务逻辑独立** - 在`lvgl_ui.c`中实现UI更新函数，不修改SquareLine生成的文件
3. **停止动画再切换** - 调用`lv_anim_del()`停止旧动画
4. **强制刷新** - UI更新后调用`lv_refr_now()`
5. **加锁保护** - 所有UI操作必须在互斥锁保护下进行
6. **添加日志** - 记录每次UI更新，便于调试

### ❌ DON'T

1. ❌ 不要直接修改`ui.c`、`ui_MainScreen.c`等自动生成的文件
2. ❌ 不要假设变量名，一定要检查实际生成的代码
3. ❌ 不要忘记停止动画就切换状态
4. ❌ 不要使用`portMAX_DELAY`，应设置合理的超时时间
5. ❌ 不要在LVGL任务外直接操作UI，必须加锁

---

## 10. 参考资源

- [LVGL ESP32 Guidelines](./lvgl-esp32-guidelines.md) - LVGL v9刷新机制和常见陷阱
- [Embedded Guidelines](./embedded-guidelines.md) - ESP32-S3嵌入式开发规范
- [SquareLine Studio官方文档](https://docs.squareline.io/)
- [LVGL v9文档](https://docs.lvgl.io/master/)

---

**核心原则**: 以实际生成的代码为准，业务逻辑独立，停动画再切换，强制刷新保显示！

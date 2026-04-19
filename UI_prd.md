# UI 组件与情绪状态机实现方案
## 一、 UI 组件映射表（底层代码依据）
根据层级树（Hierarchy）导出，`ui.h` 中核心变量可直接操作：

### 🌐 状态栏图标
- `ui_label_wifi`：WiFi 图标标签，通过切换颜色（灰/绿）表示网络连通性
- `ui_label_mqtt`：MQTT/云端同步图标标签，用于表示长连接状态

### 🤖 情绪核心组件
- `ui_EmotionCircle`：底部圆形面板，用于控制颜色背景和尺寸（配合动画）
- `ui_uiEmotionLabel`：圆圈内部文本标签，用于显示具体表情符号

### 🎬 动作函数
- `ui_breathe_Animation(ui_EmotionCircle, 0)`：自动生成的呼吸动画启动函数

## 二、 目标视觉效果设计
通过修改同一 `EmotionCircle` 属性实现多状态效果，无需创建多个屏幕

### 状态 A：倾听/待机模式（STATE_LISTENING / STATE_IDLE）
**适用场景**：智能体通过 I2S 和 VAD 拾音、安静等待
**视觉表现**：
- 形状：固定 120x120 静态正圆形，停止呼吸动画
- 颜色：柔和浅灰色/纯白色
- 内容：内部显示 `...`，模拟专注倾听效果

### 状态 B：说话/思考模式（STATE_SPEAKING / STATE_CLOUD_SYNC）
**适用场景**：云端大模型返回回复、ESP32 解码播放音频/数据处理
**视觉表现**：
- 形状：启动呼吸动画，圆圈规律放大缩小
- 颜色：活力绿色/蓝色/金色（按情绪自定义）
- 内容：清空内部文本，化身为跳动核心

## 三、 C 语言逻辑实现方案（ESP-IDF 工程）
### 1. 引入依赖
在 `main/lvgl_ui.c` 文件顶部添加：
```c
#include "ui.h"
```

### 2. 初始化 UI
在 `lvgl_task` 任务中，完成屏幕及 LVGL v9 初始化后调用：
```c
ui_init(); // 渲染所有 UI 组件
```

### 3. 核心功能函数（带互斥锁保护）
**注意**：所有 UI 操作必须包裹在 `xSemaphoreTake` 和 `xSemaphoreGive` 之间
```c
// --- 网络状态更新函数 ---
void update_network_ui(bool wifi_ok, bool mqtt_ok) {
    if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
        // WiFi 状态：绿色=正常，灰色=异常
        lv_obj_set_style_text_color(ui_label_wifi, wifi_ok ? lv_color_hex(0x00FF00) : lv_color_hex(0x808080), 0);
        // MQTT 状态：蓝色=正常，灰色=异常
        lv_obj_set_style_text_color(ui_label_mqtt, mqtt_ok ? lv_color_hex(0x00A8FF) : lv_color_hex(0x808080), 0);
        xSemaphoreGive(xGuiSemaphore);
    }
}

// --- 情绪状态切换函数 ---
void change_emotion_state(int state) {
    if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
        if (state == STATE_LISTENING) {
            lv_anim_del(ui_EmotionCircle, NULL);        // 停止所有动画
            lv_obj_set_width(ui_EmotionCircle, 120);   // 固定尺寸 120x120
            lv_obj_set_height(ui_EmotionCircle, 120);
            lv_obj_set_style_bg_color(ui_EmotionCircle, lv_color_hex(0xD3D3D3), 0); // 设置浅灰色
            lv_label_set_text(ui_uiEmotionLabel, "..."); // 显示倾听表情
        } 
        else if (state == STATE_SPEAKING) {
            lv_label_set_text(ui_uiEmotionLabel, "");    // 清空表情文本
            lv_obj_set_style_bg_color(ui_EmotionCircle, lv_color_hex(0x32CD32), 0); // 设置绿色
            ui_breathe_Animation(ui_EmotionCircle, 0);  // 启动呼吸动画
        }
        xSemaphoreGive(xGuiSemaphore);
    }
}
```

### 4. 状态机联动
在 `state_monitor_task` 中监听事件组消息，检测到音频输入结束、云端请求时，调用状态切换函数：
```c
// 示例调用
change_emotion_state(STATE_SPEAKING);
```
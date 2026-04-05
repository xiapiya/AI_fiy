# ESP32-S3 Embedded Development Guidelines

> 嵌入式硬件开发规范(基于PRD/Epic/Story)

---

## Overview

**硬件平台**: ESP32-S3 (推荐ESP32-S3-N16R8: 16MB Flash + 8MB PSRAM)  
**开发框架**: ESP-IDF / Arduino  
**核心功能**: I2S音频 + SPI TFT + OV2640摄像头 + MQTT + HTTP + LVGL UI  

**核心原则**: **禁止造轮子,优先复用官方blog_4+blog_6代码**

---

## 必须复用的官方代码

### 1. 音频处理 + MQTT (blog_4)

**官方仓库**: `emqx/esp32-mcp-mqtt-tutorial/samples/blog_4`

**复用内容**:
- I2S麦克风驱动(INMP441: BCLK=7, LRCL=8, DOUT=9)
- I2S扬声器驱动(MAX98357A: BCLK=2, LRCL=1, DOUT=42)
- VAD能量检测触发录音  
- MQTT客户端(PubSubClient或类似库)
- Base64音频编码/解码
- MP3解码播放

**禁止自己实现**: I2S驱动、MQTT客户端、音频编解码

### 2. 摄像头 + HTTP上传 (blog_6)

**官方仓库**: `emqx/esp32-mcp-mqtt-tutorial/samples/blog_6`

**复用内容**:
- OV2640摄像头DVP驱动
- JPEG压缩
- HTTP POST图像上传
- 拍照触发机制

**禁止自己实现**: 摄像头驱动、JPEG编码

---

## State Machine (状态机)

### 核心状态

```cpp
enum SystemState {
    IDLE,           // 待机(呼吸动画)
    RECORDING,      // 录音中(LED闪烁)
    CAPTURING,      // 拍照中(闪光动画)
    THINKING,       // 推理中("神经元链接中..."动画)
    PLAYING,        // 播放中(音频+UI同步)
    COOLDOWN        // 冷却期(3秒,屏蔽麦克风输入)
};
```

### PRD要求

- **播放后强制冷却3秒**: 清空音频缓冲区,屏蔽麦克风,防止回声
- **LED状态反馈**: 待机呼吸、录音闪烁、推理掩盖动画
- **MQTT断线重连**: 内置自动重连机制

---

## LVGL UI (TFT屏幕)

**库**: LVGL v9.6 + TFT_eSPI  
**性能优化**:
- 启用PSRAM全帧缓冲(`CONFIG_SPIRAM_MODE_OCT=y`)
- IRAM加速(`CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y`)
- CPU 240MHz + DMA传输

### 表情映射

| 情绪标签 | 表情文件 | 触发条件 |
|---------|---------|---------|
| happy | happy.png | AI回复包含"开心" |
| sad | sad.png | AI回复包含"难过" |
| comfort | comfort.png | AI回复包含"理解"、"陪伴" |
| thinking | loading.gif | 推理中动画 |

---

## MQTT Topics

```
emqx/esp32/{device_id}/audio       # 上传录音(Base64)
emqx/esp32/{device_id}/playaudio   # 下发播放音频(MP3 Base64)
emqx/esp32/{device_id}/ui          # UI状态更新(JSON)
emqx/esp32/{device_id}/control     # 控制指令(远程拍照、静音)
```

---

## 核心原则

### ✅ DO

- 优先复用官方blog_4+blog_6代码
- 严格遵循状态机,避免状态混乱
- 播放后强制3秒冷却
- MQTT断线自动重连

### ❌ DON'T

- ❌ 不要自己写I2S/摄像头驱动
- ❌ 不要跳过冷却期(会导致回声)
- ❌ 不要在PSRAM<8MB的芯片上跑LVGL全帧缓冲

---

**总结**: 复用官方代码 + 状态机管理 + LVGL UI + 性能优化

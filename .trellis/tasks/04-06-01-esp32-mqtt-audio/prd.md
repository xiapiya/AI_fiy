# Phase 1: ESP32-S3 MQTT音频通信基础

## Goal

实现ESP32-S3的音频采集、MQTT通信、音频播放完整闭环，为后续多模态功能打基础。

## Requirements

### 1. 硬件配置
- **开发板**: ESP32-S3-N16R8（16MB Flash + 8MB PSRAM）
- **麦克风**: INMP441（I2S接口）
  - BCLK=7, LRCL=8, DOUT=9
- **功放**: MAX98357A（I2S接口）
  - BCLK=2, LRCL=1, DOUT=42
- **扬声器**: 3W
- **LED**: 板载LED用于状态指示

### 2. 核心功能

#### 2.1 音频采集
- 采样率：8000Hz
- 位深：16bit
- 声道：单声道（Mono）
- 录音时长：固定3秒
- VAD触发：通过能量检测自动触发录音
- 编码：Base64编码后上传

#### 2.2 MQTT通信
- **Broker**: EMQX（本地部署）
- **上传主题**: `emqx/esp32/audio`
- **下发主题**: `emqx/esp32/playaudio`
- **断线重连**: 自动重连机制
- **消息格式**: Base64编码的音频数据

#### 2.3 音频播放
- 接收MQTT下发的Base64音频数据
- 解码为PCM数据
- 通过I2S输出到MAX98357A功放
- 播放后清空缓冲区

#### 2.4 状态管理
- **待机状态**: 麦克风监听，LED常亮
- **录音状态**: 录制3秒音频，LED闪烁
- **上传状态**: 发送MQTT消息，LED快闪
- **播放状态**: 播放音频，LED常亮
- **冷却期**: 播放后3秒屏蔽麦克风输入

## Technical Notes

### 复用代码
**必须复用**: EMQX官方 `esp32-mcp-mqtt-tutorial/samples/blog_4` 代码
- 仓库地址: https://github.com/emqx/esp32-mcp-mqtt-tutorial
- 路径: `samples/blog_4/`
- 包含内容：ESP32-S3 + INMP441 + MAX98357A + MQTT + Qwen-Omni集成

### 开发环境
- Arduino IDE / PlatformIO
- ESP32-S3开发板驱动
- 所需库：
  - WiFi.h
  - PubSubClient.h（MQTT客户端）
  - I2S相关库
  - Base64编码库

### 引脚配置
```cpp
// 麦克风 (INMP441) - I2S_NUM_0
#define I2S_MIC_BCLK 7
#define I2S_MIC_LRCL 8
#define I2S_MIC_DOUT 9

// 功放 (MAX98357A) - I2S_NUM_1
#define I2S_SPK_BCLK 2
#define I2S_SPK_LRCL 1
#define I2S_SPK_DOUT 42
```

## Acceptance Criteria

- [x] ESP32-S3连接WiFi成功
- [x] 连接本地EMQX Broker成功
- [x] VAD能量检测触发录音
- [x] 录制3秒音频并Base64编码
- [x] 通过MQTT上传音频数据
- [x] 接收MQTT下发的音频数据
- [x] 解码并播放音频
- [x] LED状态指示正常
- [x] 播放后进入3秒冷却期
- [x] 断线自动重连
- [ ] 通过lint检查（如有）

## Out of Scope (本阶段不做)

- TFT屏幕显示
- LVGL UI
- OV2640摄像头
- Android App
- 后端Python服务（Phase 1先用EMQX Webhook测试）

## Development Type

**embedded** - ESP32-S3嵌入式开发

# Phase 1: ESP32-S3 MQTTS音频通信基础 (V4.0云端架构版)

## Goal

实现ESP32-S3的音频采集、**MQTTS加密通信**、音频播放完整闭环，基于**纯ESP-IDF框架**和**云端EMQX**部署，为后续多模态功能打基础。

**V4.0核心升级**:
- 从Arduino升级到**纯ESP-IDF (v5.x)**
- 从MQTT明文升级到**MQTTS加密通信**
- 从本地Broker升级到**云端EMQX (公网部署)**
- 新增**mbedTLS内存优化**配置，防止TLS握手OOM

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

#### 2.2 MQTTS通信 (V4.0公网加密)
- **Broker**: EMQX云端部署（通过1Panel管理）
- **协议**: MQTTS (MQTT over TLS/SSL)
- **端口**: 8883 (加密端口)
- **认证**: 用户名+密码 或 Token认证
- **上传主题**: `{product}/esp32/{device_id}/audio`
- **下发主题**: `{product}/esp32/{device_id}/playaudio`
- **状态主题**: `{product}/esp32/{device_id}/status` (V4.0新增)
- **控制主题**: `{product}/esp32/{device_id}/control` (V4.0新增，接收远程指令)
- **断线重连**: esp_mqtt_client内置自动重连
- **消息格式**: Base64编码的音频数据
- **证书**: 烧录云端CA证书，使用mbedTLS验证

#### 2.3 音频播放
- 接收MQTT下发的Base64音频数据
- 解码为PCM数据
- 通过I2S输出到MAX98357A功放
- 播放后清空缓冲区

#### 2.4 状态管理 (FreeRTOS Event Group)
**基础状态**:
- **待机状态** (IDLE): 麦克风监听，LED常亮
- **录音状态** (RECORDING): 录制3秒音频，LED闪烁
- **上传状态** (UPLOADING): 发送MQTTS消息，LED快闪
- **播放状态** (PLAYING): 播放音频，LED常亮
- **冷却期** (COOLDOWN): 播放后3秒屏蔽麦克风输入

**V4.0新增状态** (公网通信):
- **TLS握手** (TLS_HANDSHAKE): MQTTS连接建立，显示"安全连接中..."
- **云端同步** (CLOUD_SYNC): 音频上传到云端，显示"上行同步中..."

**状态机实现**: 使用FreeRTOS Event Group管理状态位

## Technical Notes

### 复用代码 (V4.0规范)
**必须复用**: EMQX官方 `esp32-mcp-mqtt-tutorial/samples/blog_4` **业务逻辑**
- 仓库地址: https://github.com/emqx/esp32-mcp-mqtt-tutorial
- 路径: `samples/blog_4/`
- ⚠️ **重要**: 官方代码基于Arduino，**必须用ESP-IDF API重写**
- 复用内容：
  - VAD能量检测逻辑
  - Base64编解码逻辑
  - 音频流处理流程
- 禁止内容：
  - ❌ 不要直接复制Arduino代码
  - ❌ 不要使用PubSubClient库
  - ❌ 不要使用Arduino I2S库

### 开发框架 (V4.0强制要求)
- **框架**: 纯ESP-IDF v5.x (**严禁使用Arduino**)
- **构建系统**: CMake + idf.py
- **调试工具**: idf.py monitor
- **必须使用的ESP-IDF组件**:
  - `driver/i2s_std.h` - I2S标准驱动 (v5.x)
  - `mqtt/mqtt_client.h` - MQTT客户端 (支持TLS)
  - `mbedtls/base64.h` - Base64编解码
  - `esp_wifi.h` - WiFi连接
  - `freertos/FreeRTOS.h` - 任务调度
  - `esp_crt_bundle.h` - CA证书捆绑包

### mbedTLS内存配置 (V4.0 CRITICAL!)
**必须配置** (sdkconfig):
```ini
# 强制mbedTLS内存分配到PSRAM (防止TLS握手OOM)
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

### 引脚配置 (ESP-IDF版本)
```c
// 麦克风 (INMP441) - I2S_NUM_0
#define I2S_MIC_BCLK   GPIO_NUM_7
#define I2S_MIC_WS     GPIO_NUM_8
#define I2S_MIC_DIN    GPIO_NUM_9

// 功放 (MAX98357A) - I2S_NUM_1
#define I2S_SPK_BCLK   GPIO_NUM_2
#define I2S_SPK_WS     GPIO_NUM_1
#define I2S_SPK_DOUT   GPIO_NUM_42
```

### MQTTS证书配置
**证书烧录** (CMakeLists.txt):
```cmake
# 嵌入CA证书
target_add_binary_data(${PROJECT_NAME}.elf "certs/ca.pem" TEXT)
```

**代码中使用**:
```c
extern const uint8_t ca_pem_start[] asm("_binary_ca_pem_start");
extern const uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");

esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = "mqtts://your-domain.com:8883",
    .broker.verification.certificate = (const char *)ca_pem_start,
    .credentials.username = "esp32_device",
    .credentials.password = "your_password",
};
```

## Acceptance Criteria

**基础功能**:
- [ ] ESP32-S3连接WiFi成功
- [ ] 使用**纯ESP-IDF**编译通过 (无Arduino依赖)
- [ ] PSRAM初始化成功，可用内存充足
- [ ] VAD能量检测触发录音
- [ ] 录制3秒音频并Base64编码
- [ ] LED状态指示正常
- [ ] 播放后进入3秒冷却期

**V4.0新增 - MQTTS加密通信**:
- [ ] **TLS握手成功**，无OOM错误
- [ ] 连接**云端EMQX** (mqtts://端口8883) 成功
- [ ] CA证书验证通过
- [ ] 通过MQTTS上传音频数据到公网主题 `{product}/esp32/{device_id}/audio`
- [ ] 接收MQTTS下发的音频数据从 `{product}/esp32/{device_id}/playaudio`
- [ ] 解码并播放音频
- [ ] MQTTS断线自动重连
- [ ] 上报设备状态到 `{product}/esp32/{device_id}/status`

**内存与性能**:
- [ ] mbedTLS内存配置正确 (分配到PSRAM)
- [ ] TLS握手过程中无看门狗重启
- [ ] FreeRTOS任务栈无溢出
- [ ] 音频录播无卡顿或失真

**代码质量**:
- [ ] 所有代码注释使用中文
- [ ] 无Arduino库依赖
- [ ] 使用ESP-IDF原生API (`i2s_std.h`, `mqtt_client.h`)
- [ ] 通过 `idf.py build` 编译检查

## Out of Scope (本阶段不做)

**Phase 1不包含**:
- TFT屏幕显示
- LVGL UI
- OV2640摄像头
- HTTPS图片上传
- Android App
- Web监控端

**云端部分**:
- Phase 1依赖云端EMQX已部署 (参考 `.trellis/spec/backend/cloud-deployment.md`)
- FastAPI后端暂时不需要 (可用EMQX Webhook测试音频流)
- NewAPI网关暂时不需要 (Phase 2再集成大模型)

## Development Type

**embedded** - ESP32-S3嵌入式开发 (纯ESP-IDF)

## 相关文档

- **开发规范**: `.trellis/spec/guides/embedded-guidelines.md` (V4.0)
- **云端部署**: `.trellis/spec/backend/cloud-deployment.md`
- **Epic文档**: 根目录 `Epic 文档：ESP32-S3 多模态情感陪伴智能体（V4.0 云端架构演进版）.md`
- **User Stories**: 根目录 `User Stories：ESP32-S3 多模态情感陪伴智能体（V4.0 云端演进版）.md`

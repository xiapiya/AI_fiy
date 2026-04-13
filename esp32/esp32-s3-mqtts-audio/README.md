# ESP32-S3 MQTTS音频通信系统 V4.0

基于ESP-IDF的MQTTS加密音频通信项目，用于ESP32-S3多模态情感陪伴智能体。

## 项目概述

### 核心功能
- I2S音频采集 (INMP441麦克风)
- I2S音频播放 (MAX98357A功放)
- MQTTS加密通信 (连接云端EMQX)
- VAD语音活动检测
- FreeRTOS状态机管理

### 硬件要求
- **开发板**: ESP32-S3-N16R8 (16MB Flash + 8MB PSRAM)
- **麦克风**: INMP441 (I2S)
- **功放**: MAX98357A (I2S)
- **扬声器**: 3W

### 软件框架
- **开发框架**: ESP-IDF v5.x (纯C实现)
- **RTOS**: FreeRTOS
- **通信协议**: MQTTS (MQTT over TLS)

## 快速开始

### 1. 环境准备

确保已安装ESP-IDF v5.x:

```bash
cd ~/esp32-dev/esp-idf
./install.sh
source export.sh
```

### 2. 配置项目

编辑 `main/main.c` 修改WiFi配置:

```c
#define WIFI_SSID      "your_wifi_ssid"      // 修改为实际WiFi SSID
#define WIFI_PASSWORD  "your_wifi_password"  // 修改为实际WiFi密码
```

编辑 `main/mqtt_client.h` 修改MQTT配置:

```c
#define MQTT_BROKER_URI    "mqtts://your-domain.com:8883"  // 云端EMQX地址
#define MQTT_USERNAME      "esp32_device"                  // MQTT用户名
#define MQTT_PASSWORD      "your_password"                 // MQTT密码
#define MQTT_CLIENT_ID     "esp32_s3_001"                  // 设备ID
```

替换 `certs/ca.pem` 为实际的云端CA证书。

### 3. 编译项目

```bash
cd /home/xiapiya/AI_FLY/esp32/esp32-s3-mqtts-audio
idf.py build
```

### 4. 烧录固件

```bash
idf.py -p /dev/ttyUSB0 flash
```

### 5. 查看日志

```bash
idf.py -p /dev/ttyUSB0 monitor
```

## 引脚配置

### 麦克风 (INMP441)
| 引脚 | GPIO |
|------|------|
| BCLK | GPIO7 |
| WS   | GPIO8 |
| DIN  | GPIO9 |

### 功放 (MAX98357A)
| 引脚 | GPIO |
|------|------|
| BCLK | GPIO2 |
| WS   | GPIO1 |
| DOUT | GPIO42 |

## MQTT主题

| 主题 | 方向 | 说明 |
|------|------|------|
| `aifly/esp32/{device_id}/audio` | 上传 | 录音数据(Base64) |
| `aifly/esp32/{device_id}/playaudio` | 下发 | 播放音频(Base64) |
| `aifly/esp32/{device_id}/status` | 上传 | 设备状态 |
| `aifly/esp32/{device_id}/control` | 下发 | 控制指令 |

## 系统状态

系统使用FreeRTOS Event Group管理状态:

- **IDLE**: 待机状态，监听麦克风
- **RECORDING**: 录音中 (3秒)
- **TLS_HANDSHAKE**: TLS握手中
- **CLOUD_SYNC**: 云端同步中
- **PLAYING**: 播放音频中
- **COOLDOWN**: 冷却期 (播放后3秒)

## 配置说明

### mbedTLS内存优化

`sdkconfig.defaults` 中已配置mbedTLS使用PSRAM，防止TLS握手时OOM:

```ini
CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=16384
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=4096
CONFIG_MBEDTLS_DYNAMIC_BUFFER=y
CONFIG_MBEDTLS_DYNAMIC_FREE_PEER_CERT=y
```

### PSRAM配置

启用8MB PSRAM八线模式:

```ini
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```

## 故障排查

### 编译错误

确保ESP-IDF环境已正确安装:

```bash
idf.py --version
```

### MQTT连接失败

1. 检查WiFi配置是否正确
2. 检查MQTT Broker地址和端口
3. 检查CA证书是否正确
4. 查看日志中的TLS握手信息

### 音频无声

1. 检查I2S引脚连接
2. 检查功放供电
3. 检查音量设置
4. 使用示波器检查I2S信号

### 内存不足

确保PSRAM已正确初始化，查看日志中的内存信息。

## 项目结构

```
esp32-s3-mqtts-audio/
├── CMakeLists.txt          # 项目配置
├── sdkconfig.defaults      # 默认配置
├── README.md               # 本文档
├── certs/                  # 证书目录
│   └── ca.pem              # CA证书
└── main/                   # 主程序
    ├── CMakeLists.txt      # 组件配置
    ├── main.c              # 主程序
    ├── i2s_audio.c/.h      # I2S音频驱动
    ├── mqtt_client.c/.h    # MQTT客户端
    ├── vad.c/.h            # VAD检测
    └── state_machine.c/.h  # 状态机
```

## 开发规范

本项目遵循 `.trellis/spec/guides/embedded-guidelines.md` 规范:

- ✅ 纯ESP-IDF开发，无Arduino依赖
- ✅ 所有注释使用中文
- ✅ 模块化设计，职责清晰
- ✅ 使用FreeRTOS进行任务管理
- ✅ 启用MQTTS加密通信

## 版本信息

- **版本**: V4.0
- **框架**: ESP-IDF v5.x
- **开发板**: ESP32-S3-N16R8
- **日期**: 2025-04

## 后续开发

Phase 2 将增加:
- TFT屏幕显示 (LVGL UI)
- OV2640摄像头
- HTTPS图片上传
- 多模态情感交互

## 参考资料

- [ESP-IDF编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)
- [EMQX MQTT文档](https://www.emqx.io/docs/)
- PRD文档: `.trellis/tasks/04-06-01-esp32-mqtt-audio/prd.md`
- 开发规范: `.trellis/spec/guides/embedded-guidelines.md`

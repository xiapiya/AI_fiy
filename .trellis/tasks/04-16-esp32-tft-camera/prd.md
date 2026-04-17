# ESP32-S3 TFT屏幕和摄像头集成

## 目标
为ESP32-S3情感陪伴智能体集成TFT屏幕和OV2640摄像头，实现情感UI可视化和远程视觉抓拍功能。

## 开发类型
**嵌入式开发** (Embedded - ESP-IDF)

## 分阶段实施

### Phase 1: TFT屏幕 ✅ 当前任务 (无依赖，可立即开始)
- ESP-LCD驱动集成
- LVGL v9+图形库
- 情感UI实现 (5种表情+字幕+网络状态)
- 状态同步 (监听状态机事件)
- **验收**: 帧率≥30FPS，无内存泄漏，不阻塞音频

### Phase 2: 后端Vision接口 ⏸️ 阻塞Phase 3 (需创建独立任务)
- 实现 `POST /api/v1/vision/upload` 接口
- 集成NewAPI网关调用Qwen-VL
- 实现TTS音频合成
- 实现MQTT音频下发
- **验收**: 图片上传→视觉识别→音频下发全链路<3秒

### Phase 3: 摄像头 ⏳ 等待Phase 2 (依赖后端接口)
- OV2640驱动集成
- HTTPS异步上传到后端
- 远程抓拍指令处理
- **验收**: 抓拍+上传<3秒，JPEG<300KB，不阻塞音频/UI

---

## Phase 1: TFT屏幕需求

### 功能需求
- [ ] 显示5种情绪表情 (happy/sad/comfort/thinking/neutral)
- [ ] 显示网络状态栏 (WiFi/MQTTS连接图标)
- [ ] 显示AI回复字幕
- [ ] 平滑动画过渡 (≥30FPS)
- [ ] 与状态机同步 (IDLE/RECORDING/PLAYING/CLOUD_SYNC)

### 技术需求
- [ ] 使用ESP-IDF的`esp_lcd`组件 (弃用Arduino TFT_eSPI)
- [ ] 集成LVGL v9+
- [ ] PSRAM Double Buffer
- [ ] FreeRTOS Mutex保护LVGL渲染
- [ ] 独立的LVGL任务 (不阻塞音频)

### 硬件配置 (已确认)
- **TFT屏幕型号**: 2.2寸 ILI9341 SPI模块 (MSP2202)
- **屏幕尺寸**: 2.2寸
- **分辨率**: 240x320 像素
- **驱动IC型号**: ILI9341
- **是否带触摸屏**: 否
- **工作电压**: 3.3V~5V (IO电平3.3V)
- **颜色**: RGB 65K色
- **资料链接**: https://www.lcdwiki.com/zh/2.2inch_SPI_Module_ILI9341_SKU%3AMSP2202

### SPI引脚配置 (已确认 - 方案A)
```
TFT屏幕:
├── SCK (时钟):     GPIO 13
├── MOSI (数据输出): GPIO 11
├── CS (片选):      GPIO 10
├── DC (数据/命令):  GPIO 12  ← 修改后避免冲突
├── RST (复位):     GPIO 3   ← 修改后避免冲突
└── BL (背光):      GPIO 5

已占用引脚 (I2S音频):
├── INMP441: GPIO 7(BCLK), 8(WS), 9(DIN)
└── MAX98357A: GPIO 2(BCLK), 1(WS), 4(DOUT)
```

---

## Phase 2: 摄像头需求

### 功能需求
- [ ] 远程抓拍 (响应Android App MQTT指令)
- [ ] HTTPS上传到云端
- [ ] JPEG压缩 (<300KB)
- [ ] 抓拍+上传 <3秒

### 技术需求
- [ ] 集成官方`esp32-camera`组件
- [ ] 使用`esp_http_client`异步POST
- [ ] PSRAM存储图片缓冲区
- [ ] 独立的抓拍任务 (不阻塞音频/UI)

### 硬件配置 (已确认)
- **摄像头型号**: OV2640独立模块
- **图片分辨率**: 800x600 (SVGA)
- **资料路径**: `/home/xiapiya/AI_FLY/0210-OV2640&OV5640摄像头模块（蓝色）客户资料(1)`

### 摄像头引脚配置 (方案A - 已修正冲突)
```
OV2640摄像头:
├── XCLK:  GPIO 15
├── PCLK:  GPIO 18
├── VSYNC: GPIO 16
├── HREF:  GPIO 17
├── SDA:   GPIO 38
├── SCL:   GPIO 39
├── D0:    GPIO 40
├── D1:    GPIO 41
├── D2:    GPIO 42
├── D3:    GPIO 21  ← 修改 (原GPIO 2冲突)
├── D4:    GPIO 6
├── D5:    GPIO 47  ← 修改 (原GPIO 7冲突)
├── D6:    GPIO 48  ← 修改 (原GPIO 8冲突)
└── D7:    GPIO 14

⚠️ 原始冲突 (已修正):
  - D3: GPIO 2  (与MAX98357A BCLK冲突) → GPIO 21
  - D5: GPIO 7  (与INMP441 BCLK冲突)   → GPIO 47
  - D6: GPIO 8  (与INMP441 WS冲突)     → GPIO 48
```

### 云端上传配置 (待后端实现)
- **上传端点**: `POST https://{domain}/api/v1/vision/upload`
- **鉴权方式**: JWT/Token (待后端实现时确定)
- **依赖**: 后端FastAPI vision接口 (Phase 2开发)

---

## 验收标准

### TFT屏幕
- [ ] 帧率 ≥30FPS
- [ ] 无内存泄漏
- [ ] 不阻塞音频任务
- [ ] LVGL渲染无看门狗超时

### 摄像头
- [ ] 抓拍+上传 <3秒
- [ ] JPEG <300KB
- [ ] 不阻塞音频/UI任务
- [ ] 无内存泄漏

---

## 技术风险
- **mbedTLS OOM**: TLS握手需大量内存，必须使用PSRAM
- **LVGL线程安全**: 必须用Mutex保护所有UI操作
- **内存碎片化**: 大量malloc/free可能导致碎片化
- **引脚冲突**: TFT SPI和摄像头可能共用引脚

---

## 依赖项
- 已完成: WiFi、MQTTS、I2S音频、VAD、状态机
- 待完成: TFT驱动、LVGL集成、摄像头驱动、HTTPS上传

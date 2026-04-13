# 更新项目文档到V4.0云端架构演进版

## Goal

将.trellis/spec/目录下的所有项目规范文档从V3.2升级到V4.0，反映以下核心架构演进：
- 从局域网升级到公网加密通信（MQTTS + HTTPS）
- 新增云端容器化架构（1Panel + NewAPI + FastAPI）
- 新增多端协同（Web监控端 + Android移动端）
- 优化内存管理（MbedTLS分配到PSRAM）
- 增强视觉多模态能力（OV2640 + Qwen-VL）

## Requirements

### 1. 嵌入式开发规范更新

文件：`.trellis/spec/guides/embedded-guidelines.md`

**从V3.2升级到V4.0，新增内容**：

#### 1.1 公网安全通信
- **MQTTS加密配置**：
  - 使用`esp_mqtt_client` + mbedTLS
  - 烧录云服务器根证书
  - 启用TLS/SSL加密
  - 处理TLS握手内存峰值

- **HTTPS图片上传**：
  - 使用`esp_http_client` + SSL
  - POST方式上传JPEG到云端FastAPI
  - 携带JWT/Token鉴权
  - 图片大小<300KB，上传<3s完成

#### 1.2 内存管理优化（新增CRITICAL章节）
- **MbedTLS内存配置**：
  - 必须在`menuconfig`中强制分配到PSRAM
  - 防止TLS握手时OOM导致重启
  - RSA加解密需要30-40KB连续内存
  - 配置项：`CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN`等

#### 1.3 状态机扩展
新增状态：
- `STATE_TLS_HANDSHAKE_BIT` - TLS握手中
- `STATE_CLOUD_SYNC_BIT` - 云端同步中
- `STATE_UPLOADING_BIT` - 图片上传中

#### 1.4 MQTT Topics更新
更新为公网鉴权分层主题：
```
{product}/esp32/{device_id}/audio       # ESP32上传录音
{product}/esp32/{device_id}/playaudio   # 云端下发音频
{product}/esp32/{device_id}/ui          # 云端下发UI状态
{product}/esp32/{device_id}/control     # 远程控制（拍照/静音）
{product}/esp32/{device_id}/status      # 设备状态心跳
emqx/system/logs                        # 全局日志
```

### 2. 后端开发规范补充

文件：`.trellis/spec/backend/index.md` 及相关文档

**新增云端架构规范**：

#### 2.1 目录结构（新增或更新）
文件：`.trellis/spec/backend/directory-structure.md`

补充云端FastAPI项目结构：
```
backend/
├── app/
│   ├── api/
│   │   └── v1/
│   │       ├── audio.py      # 音频处理路由
│   │       └── vision.py     # 视觉处理路由
│   ├── services/
│   │   ├── newapi.py         # NewAPI网关调度
│   │   ├── tts.py            # TTS合成服务
│   │   └── mqtt.py           # MQTT下发服务
│   ├── models/               # 数据模型
│   └── core/
│       └── config.py         # 配置管理
├── docker-compose.yml        # 1Panel部署配置
└── requirements.txt
```

#### 2.2 NewAPI网关规范（新建章节）
- **路由策略**：后端不直接请求厂商API，统一打向NewAPI
- **接口格式**：兼容OpenAI `/v1/chat/completions`
- **模型配置**：
  - Qwen-VL-Max：视觉语义提取
  - Qwen-VL-Plus：视觉语义提取（备用）
  - Qwen-Omni：语音对话
- **密钥管理**：在NewAPI统一配置，后端使用内网端口
- **并发控制**：网关层面控制请求并发

#### 2.3 错误处理（更新）
文件：`.trellis/spec/backend/error-handling.md`

新增云端特有错误处理：
- NewAPI网关超时处理
- MQTT下发失败重试机制
- TTS服务降级策略
- 公网连接异常处理

#### 2.4 日志规范（更新）
文件：`.trellis/spec/backend/logging-guidelines.md`

新增云端日志：
- API调用耗时统计
- MQTT下发状态追踪
- 多模态处理链路日志
- 下发到`emqx/system/logs`的日志格式

### 3. 前端开发规范更新

文件：`.trellis/spec/frontend/index.md`（Android部分）

**补充多端协同规范**：

#### 3.1 Android移动端（已有，需要补充）
新增功能规范：
- **远程拍照指令**：通过MQTT下发到`{product}/esp32/{device_id}/control`
- **静音硬件开关**：防止多设备冲突的硬件控制
- **Room本地化**：聊天记录+历史图片持久化
- **Foreground Service**：保活MQTT长连接

#### 3.2 Web监控端（新增）
新建文件：`.trellis/spec/frontend/web-monitor.md`

**技术栈**：HTML/JS + WSS (WebSocket Secure)
**部署**：Nginx挂载

**功能规范**：
- WSS连接EMQX，订阅`emqx/system/logs`
- 实时显示设备动作流
- 展示抓拍图片记录
- 耗时统计图表（全链路延迟分析）
- 只读权限，不下发控制指令

### 4. 新增云端部署指南

新建文件：`.trellis/spec/backend/cloud-deployment.md`

**内容包括**：

#### 4.1 服务器环境
- Linux云服务器配置
- 防火墙端口开放
- 域名解析与SSL证书

#### 4.2 1Panel容器化管理
- 1Panel安装与配置
- EMQX容器部署（开启TLS/SSL）
- NewAPI容器部署
- MySQL容器部署
- Nginx反向代理配置

#### 4.3 FastAPI部署
- Docker化FastAPI应用
- 环境变量管理
- 内网与NewAPI通信配置
- MQTT客户端配置

#### 4.4 安全配置
- MQTTS证书管理
- HTTPS证书管理
- JWT/Token鉴权
- API密钥保护

### 5. 跨层思维指南更新

文件：`.trellis/spec/guides/cross-layer-thinking-guide.md`

**新增V4.0公网架构的跨层检查项**：

#### 5.1 公网通信安全检查
- [ ] ESP32端是否正确配置TLS证书？
- [ ] MbedTLS内存是否分配到PSRAM？
- [ ] HTTPS接口是否携带鉴权Token？
- [ ] 敏感数据是否走加密通道？

#### 5.2 云端链路检查
- [ ] FastAPI是否通过NewAPI而非直连大模型？
- [ ] MQTT下发指令格式是否与ESP32端解析一致？
- [ ] 延迟补偿状态节点是否完整？
- [ ] 错误处理是否覆盖网络抖动场景？

#### 5.3 多端协同检查
- [ ] Android/Web端主题订阅是否正确？
- [ ] 远程指令优先级是否高于VAD触发？
- [ ] 多端并发控制是否防止冲突？

## Acceptance Criteria

- [ ] `.trellis/spec/guides/embedded-guidelines.md` 升级到V4.0，包含公网TLS、HTTPS、内存优化
- [ ] `.trellis/spec/backend/` 补充完整的云端架构规范（FastAPI + NewAPI + 1Panel）
- [ ] `.trellis/spec/backend/cloud-deployment.md` 新增云端部署指南
- [ ] `.trellis/spec/frontend/index.md` Android部分更新多端协同功能
- [ ] `.trellis/spec/frontend/web-monitor.md` 新增Web监控端规范
- [ ] `.trellis/spec/guides/cross-layer-thinking-guide.md` 新增V4.0跨层检查项
- [ ] 所有文档标注版本号（V4.0）并说明与V3.2的演进关系
- [ ] 所有文档使用中文撰写（除代码示例外）

## Technical Notes

**文档更新原则**：
1. **保留V3.2核心内容**：已经正确的ESP-IDF、FreeRTOS、LVGL部分保持不变
2. **新增V4.0特性**：在原有基础上增加公网、云端、多端相关章节
3. **标注版本演进**：在文档开头说明"V4.0云端架构演进版，基于V3.2"
4. **突出关键风险**：特别强调MbedTLS内存OOM、LVGL线程安全等CRITICAL问题
5. **复用优先**：继续强调复用官方代码、禁止造轮子

**参考文档**：
- 根目录下的PRD V4.0
- 根目录下的Epic V4.0
- 根目录下的User Stories V4.0

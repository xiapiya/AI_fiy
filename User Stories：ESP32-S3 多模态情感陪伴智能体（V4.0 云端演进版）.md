

**Epic ID**：EPIC-002 **文档版本**：V4.0（基于纯 ESP-IDF 架构 + 公网加密云架构细化） **生成日期**：2026年4月 **角色**：Scrum Master（xiapiya 项目） **状态**：已细化 & 待 Sprint 规划 **目标**：将 Epic 拆分为可独立开发、验收的 User Stories，全面拥抱公网安全通信与容器化云部署。 **技术基调**：纯 ESP-IDF (v5.x) + FreeRTOS、公网 MQTTS/HTTPS 加密传输、云端 1Panel + NewAPI 统一调度。

## Feature 1：硬件底座与感知执行层（纯 ESP-IDF）

**优先级**：P0（Phase 1） | **估算**：10人天

- **User Story 1.1：I2S 音频流与 VAD 触发**
    
    - **As** 硬件开发者，**I want** 通过 `i2s_std.h` 驱动 INMP441 录音与 MAX98357A 播放，**so that** 支持基于 FreeRTOS 任务的 VAD 能量触发且适应公网网络波动的音频流。
        
    - **验收标准**：VAD 独立任务检测>阈值自动录音；针对公网延迟，加大 PSRAM 环形缓冲区 (RingBuffer) 深度；播放云端下发的 Base64 音频无卡顿、无看门狗报警。
        
    - **技术任务**：启用 ESP-IDF v5.x 原生 `driver/i2s_std.h`；独立 FreeRTOS Task 分离收发。
        
- **User Story 1.2：ESP-LCD 与 LVGL 高帧率 UI**
    
    - **As** 硬件开发者，**I want** 通过 `esp_lcd` 驱动 TFT 屏幕并运行 LVGL v9+，**so that** 提供现代化的高帧率视觉反馈。
        
    - **验收标准**：待机动效流畅（≥30FPS）；内存使用符合标准（启用 PSRAM Double Buffer）。
        
    - **技术任务**：彻底弃用 Arduino `TFT_eSPI`；严格配置 FreeRTOS Mutex (`xSemaphoreTake/Give`) 保障 LVGL 渲染线程安全。
        
- **User Story 1.3：原生摄像头驱动与公网上传**
    
    - **As** 硬件开发者，**I want** 基于原生驱动唤醒 OV2640 抓拍并经由 HTTPS 上传，**so that** 在保障公网数据隐私的前提下实现“看看我”视觉触发。
        
    - **验收标准**：抓拍+内存压缩+HTTPS 上传 < 3s 完成（JPEG < 300KB）；过程不阻塞音频/UI 任务。
        
    - **技术任务**：集成官方 `esp32-camera` 组件；调用 `esp_http_client` 异步 POST。
        
- **User Story 1.4：全生命周期状态机**
    
    - **As** 硬件开发者，**I want** 基于 FreeRTOS Event Group 构建状态机，**so that** UI 动效、公网握手、大模型推理完美解耦同步。
        
    - **验收标准**：增加“TLS 握手中”与“云端同步中”状态；状态切换无内存泄漏。
        

## Feature 2：公网加密交互与通信链路

**优先级**：P0（Phase 1-2） | **估算**：8人天（增加 SSL/TLS 调试工时）

- **User Story 2.1：混合模态防冲突队列**
    
    - **As** 系统，**I want** 统一管理 VAD 语音与移动端 MQTT 抓拍指令，**so that** 避免端侧并发执行崩溃。
        
    - **验收标准**：移动端“远程指令”优先级最高；利用 FreeRTOS Timer 实现可靠的 3s 软冷却防冲撞。
        
- **User Story 2.2：MQTTS + HTTPS 加密双通道**
    
    - **As** 系统，**I want** 轻量状态走 MQTTS，图片文件走 HTTPS，**so that** 保障公网长连接稳定性与隐私数据防篡改。
        
    - **验收标准**：公网断网自动重连极快；TLS/SSL 握手时不发生 OOM（内存溢出）。
        
    - **技术任务**：烧录云服务器根证书；**必须**在 `menuconfig` 中将 mbedTLS 内存分配强制指定到 PSRAM；使用 `esp_mqtt_client`。
        

## Feature 3：云端大脑与容器化中枢

**优先级**：P0（Phase 3） | **估算**：10人天

- **User Story 3.1：多模态路由与 API 网关分发**
    
    - **As** AI大脑，**I want** 接收边缘端数据后，通过本地内网的 NewAPI 转发至 Qwen 大模型，**so that** 实现账单收敛与密钥安全。
        
    - **验收标准**：FastAPI 后端不直连公网大模型，全部路由至部署于 1Panel 的 NewAPI 网关端口；生成≤20字回复+情绪标签 JSON。
        
    - **技术任务**：FastAPI 异步处理；对接 NewAPI 的 OpenAI 兼容格式 (`/v1/chat/completions`)；利用 Qwen-VL-Max/Plus 提取视觉语义。
        
- **User Story 3.2：云端下发与延迟补偿**
    
    - **As** AI大脑，**I want** 实时通过 MQTT 向设备推送状态并在 TTS 完成后下发音频流，**so that** 掩盖边缘端等待云端大模型推理的延迟。
        
    - **验收标准**：接收到图片后立即推送 `ui/status: processing`；Cherry 音色 MP3 < 5s 生成并下发。
        

## Feature 4：屏幕 UI 与视觉表达（映射公网状态）

**优先级**：P1（Phase 2） | **估算**：7人天

- **User Story 4.1：现代化 UI 组件**
    
    - **As** 用户，**I want** 看到包含表情、字幕及网络状态栏（WiFi/MQTTS 连接图标）的界面，**so that** 直观掌握设备在线状态与 AI 情绪。
        
    - **验收标准**：5种主情绪映射；C 数组图片资源置于 Flash 并在运行时加载至 PSRAM。
        
- **User Story 4.2：动效过渡闭环**
    
    - **As** 用户，**I want** 网络传输与推理延迟被平滑的掩盖动画过渡，**so that** 弱网环境下依然体验流畅无死机感。
        
    - **验收标准**：端侧严格解析云端下发的 JSON 同步触发表情+字幕+音频播放。
        

## Feature 5：多端协同（Web看板 + Android伴侣）

**优先级**：P1（Phase 3-4） | **估算**：12人天

- **User Story 5.1：Web 全链路监控 (WSS)**
    
    - **As** 管理员，**I want** 在挂载于 Nginx 的 Web 看板实时查看设备日志流与抓拍图，**so that** 远程监控全链路健康度与 API 耗时。
        
    - **验收标准**：使用 WSS (WebSocket Secure) 连接云端 EMQX；图像实时展示；全链路延迟图表分析。
        
- **User Story 5.2：Android 原生控制中枢**
    
    - **As** Android 开发者，**I want** 使用 Kotlin 结合 HiveMQ 构建具有工程规范的移动端，**so that** 用户可以在外远程下发指令控制留在宿舍桌面的设备。
        
    - **验收标准**：App 端点击“请求视觉”，远端硬件即刻响应抓拍；Room 本地持久化聊天记录与历史图片；Foreground Service 保活 MQTT 长连接。
        

---

### 全局 API 与通信协议规范 (更新)

- **MQTTS 主题 (TLS)**：采用标准鉴权分层 `{product}/esp32/{device_id}/{action}`。
    
- **HTTPS 接口**：`POST https://你的域名/api/v1/vision/upload`（需携带 JWT/Token 鉴权头）。
    
- **模型端点**：`http://localhost:3000/v1/chat/completions` (假设云端 NewAPI 运行于本地 3000 端口)。
    

### 风险更新（针对公网纯 ESP-IDF 架构）

1. **最大系统崩溃风险（MbedTLS OOM）**：ESP32 建立 HTTPS/MQTTS 连接时，RSA 加解密瞬间需要分配大量连续内存（约 30-40KB）。如果此时 FreeRTOS 堆栈或 PSRAM 碎片化严重，会导致 `mbedtls_ssl_handshake` 失败或直接重启。**应对方案**：强制启用动态分配并优先使用 PSRAM。
    
2. **LVGL 线程安全**：必须使用 `xSemaphoreTake` 和 `xSemaphoreGive` 包裹**所有** LVGL 的 UI 刷新任务，否则必定崩溃。
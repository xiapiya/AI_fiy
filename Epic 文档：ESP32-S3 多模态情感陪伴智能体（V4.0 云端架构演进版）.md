# Epic 任务书：xiapiya 情感陪伴体落地

**Epic ID**：EPIC-002
**所属系列**：从0到1打造“情感陪伴智能体”系列（综合落地指南）
**项目代号**：xiapiya
**文档版本**：V4.1（基于纯 ESP-IDF 架构、云端服务器公网部署与极简几何UI方案）
**产品负责人**：Scrum Master
**状态**：已确认 & 技术栈更新（2026.04）

**目标交付**：一款具备“听、说、看、表达”四维能力的桌面具身情感陪伴智能体，实现基于公网的语音+视觉多模态触发、**TFT极简几何UI反馈**、多端（Web/App）实时跨网协同。

**预计价值**：从局域网纯语音升级为公网多模态，底层采用工业级 ESP-IDF 与 FreeRTOS 原生调度，结合云端容器化架构（1Panel + Docker），大幅提升系统稳定性、公网安全性与多任务并发能力。**引入极简 UI 策略，极大降低边缘端图形渲染开销，保障核心通信链路的极致流畅。**

## 1. Epic 背景与核心目标

- **业务价值：** 基于 ESP32-S3 边缘侧强大算力（16MB Flash + 8MB PSRAM），结合云端网关统一调度的 Qwen-VL + Omni 大模型，打造极具商业落地潜力的桌面具身智能终端。
- **核心架构：**
    - **边缘执行层（纯 ESP-IDF Native）：** ESP32-S3 + FreeRTOS 任务调度（原生 `i2s_std.h` 音频 + OV2640 DVP + `esp_lcd` 驱动 TFT 屏幕 + **SquareLine 静态布局 + LVGL 动效**）。
    - **公网通信中枢：** EMQX MQTTS (`esp_mqtt` + TLS) + HTTPS (`esp_http_client` + SSL) 加密双通道。
    - **云端逻辑大脑：** Linux 云服务器 + 1Panel 面板统筹部署（FastAPI 路由处理 + NewAPI 模型网关 + TTS 转换）。
    - **监控/伴侣端：** Web 端只读看板（部署于 Nginx）+ Android 移动端（HiveMQ 接入）。
- **非功能需求：** 公网低延迟（视觉触发全链路 < 8s）、高并发（FreeRTOS 独立队列管理与云端网关并发控制）、端到端加密安全、成本可控。
- **成功标准：** Phase 2 跑通基于 FreeRTOS 与公网 MQTT 的单模态闭环；Phase 4 完成多模态云端全链路，软硬件多端无缝跨网协同。

## 2. 类似项目参考（调整复用策略）

| **排名** | **项目/参考源** | **核心借鉴点** | **在云端+ESP-IDF 环境下的对策** |
| :--- | :--- | :--- | :--- |
| 1 | EMQX官方教程 (blog_4 & 6) | 双通道架构逻辑、Qwen大模型Prompt策略 | 提炼 JSON 解析逻辑。使用 ESP-IDF `mqtt_client.h` 并必须启用 mbedTLS 证书校验，直连云端 EMQX 容器。 |
| 2 | ESP-IDF 官方 LVGL 移植示例 | 驱动 SPI TFT 屏幕的标准工业级做法 | 弃用 Arduino `TFT_eSPI`，直接复用 IDF 的 `esp_lcd` 面板驱动结合 LVGL 库，利用 PSRAM 缓存。 |
| 3 | esp-who / esp-iot-solution | OV2640 的原生 C 语言驱动与 JPEG 压缩 | 引入官方 `camera_driver` 组件，配置 JPEG 压缩率以平衡网络上传带宽与图像清晰度。 |

## 3. 需求拆分（Feature / User Story 层级）

### Feature 1：硬件底座与感知执行层（ESP32-S3 + ESP-IDF）
- **优先级：** P0（Phase 1）
- **用户故事：**
    - As 硬件，我能基于 FreeRTOS 任务队列，独立运行 I2S 录播且互不阻塞。
    - As 硬件，我能通过 `esp_lcd` 驱动 TFT 屏幕并运行 LVGL 渲染动态 UI。
    - As 硬件，接收到云端下发的指令时，唤醒 OV2640 抓拍并压缩为 JPEG。
- **技术要求：** 纯 ESP-IDF v5.x 开发、FreeRTOS 多任务设计 (RingBuffer/Queue)、`i2s_std.h`。
- **验收标准：** 硬件初始化正常，FreeRTOS 任务调度无看门狗复位，音频录播无底层报错。

### Feature 2：公网安全通信链路（MQTTS + HTTPS）
- **优先级：** P0（Phase 1-2）
- **用户故事：**
    - As 系统，轻量控制状态走加密的 MQTTS，抓拍的大体积 JPEG 走 HTTPS POST 直传云端，防篡改与窃听。
- **技术要求：** 使用 `esp_mqtt_client` 和 `esp_http_client`，**必须集成 mbedTLS 烧录云端根证书**；处理好 SSL 握手时的内存峰值（分配至 PSRAM）。
- **验收标准：** 断网或公网抖动时自动重连恢复极快；高频次 TLS 加密传输中无内存溢出 (OOM)。

### Feature 3：云端大脑与基建（1Panel/Docker + FastAPI）
- **优先级：** P0（Phase 2-3）
- **功能：**
    - 利用 1Panel 在服务器部署 EMQX、NewAPI 等基础设施。
    - FastAPI 暴露 HTTPS 接口，路由音频与图像。调用本地部署的 NewAPI 统一下发给 Qwen-VL/Omni，实现多模态融合并提取情绪标签。合成音频并经由云端 MQTT 广播下发。

### Feature 4：极简状态机与视觉表达（SquareLine + LVGL）
- **优先级：** P1（Phase 2-3）
- **用户故事：**
    - As 用户，我能通过屏幕顶部图标颜色的变化直观了解 WiFi 与 MQTT 的公网连接状态，而不受弹窗干扰。
    - As 用户，设备在不同生命周期（待机、倾听、思考、说话）能呈现克制且清晰的几何图形反馈（如静态省略号、呼吸圆），降低视觉负担。
- **技术要求：** 采用 SquareLine Studio 导出静态布局与状态栏组件，通过 C 语言逻辑层结合 LVGL `lv_anim_t` 实现中心图形的呼吸与闪烁动效。FreeRTOS Mutex 保障 GUI 线程安全。
- **验收标准：** 状态机与云端下发节点精确匹配，动效流畅（无掉帧），彻底消除因复杂图片切换导致的内存峰值风险。

### Feature 5：多端跨网协同（Web监控 + Android文本客户端）
- **优先级：** P1（Phase 4）
- **用户故事：**
    - As 用户，我希望在手机上通过文本与智能体对话，无需依赖桌面硬件。
    - As 用户，我希望手机端的对话能够访问桌面硬件的历史上下文（跨端记忆同步）。
    - As 管理员，我希望通过Web端实时查看云端处理日志流与抓拍画面（WSS 协议）。
- **功能：**
    - **Android文本客户端（V4.2极简解耦版）**：
        - 纯文本对话界面（流式渲染）
        - 通过HTTP/SSE与云端通信（不使用MQTT）
        - 硬编码Session ID（免鉴权设计）
        - Room本地持久化聊天历史
        - 完全解耦硬件控制（不触发ESP32动作）
    - **Web监控端**：
        - WSS连接EMQX，订阅系统日志
        - 实时显示云端处理链路
        - 展示抓拍图片记录
- **技术要求：** Android使用Kotlin + Retrofit + OkHttp-SSE + Room + MVVM架构；Web使用原生WebSocket API连接EMQX。
- **验收标准：** Android文本对话流畅，跨端上下文准确，Web日志实时刷新。

## 4. API 与 通信协议规范（全局共享）

### 4.1 ESP32硬件通信
- **MQTTS（轻数据）：**
    - `aifly/esp32/{device_id}/audio` - 音频上传
    - `aifly/esp32/{device_id}/playaudio` - 音频下发
    - `aifly/esp32/{device_id}/status` - 设备状态
    - `aifly/esp32/{device_id}/control` - 远程控制
    - `emqx/system/logs` - 系统日志（Web监控）
- **HTTPS（重数据）：**
    - `POST /api/v1/vision/upload` - 图片上传（携带鉴权Token + JPEG数据）

### 4.2 Android文本客户端通信
- **HTTP/SSE：**
    - `POST /api/v1/chat/text` - 文本对话（同步响应）
    - `POST /api/v1/chat/stream` - 文本对话（SSE流式响应）
- **请求格式：**
    ```json
    {
        "session_id": "xiapiya_master_01",
        "content": "用户输入文本",
        "timestamp": 1713514200
    }
    ```
- **响应格式：**
    ```json
    {
        "reply": "AI回复文本",
        "emotion": "happy",
        "timestamp": 1713514205
    }
    ```

### 4.3 云端上下文管理
- **统一Session ID：** Android的`session_id`与ESP32的`device_id`映射到同一上下文池
- **记忆存储：** Redis/MySQL存储完整对话历史（语音+文本）
- **模型网关路由：** 所有大模型请求指向服务器内网NewAPI端口，严禁边缘设备直连阿里云

## 5. 里程碑与开发路线图（敏捷迭代 V4.1版）

- **Phase 1（第1-2周）：云端基建与底层双向通信**
    - 云端：完成服务器环境配置、容器部署、域名解析与 SSL 证书配置。
    - 边缘：搭建 ESP-IDF 工程框架，分配 FreeRTOS 任务栈，跑通带 mbedTLS 的原生 MQTTS 通信。
- **Phase 2（第3-4周）：极简 UI 状态机与单模态闭环**
    - 边缘：集成 SquareLine 导出的静态 UI 代码，手写状态机动画（呼吸圆/省略号）；对接基础音频模块。
    - 云端：开发 FastAPI 纯语音处理链路，完成单模态对话跨网闭环。
- **Phase 3（第5-6周）：视觉接入与多模态中枢联调**
    - 边缘：引入官方 `camera_driver` 组件，跑通 HTTPS 上传 JPEG，增加抓拍瞬间的快门闪光 UI。
    - 云端：FastAPI 对接视觉大模型，开发 Web 实时监控看板。
- **Phase 4（第7-10周）：移动端生态与商业级调优**
    - 完成 Android 文本客户端（V4.2极简版）：
        - Kotlin + Retrofit + SSE流式渲染
        - Room本地持久化
        - MVVM架构
        - 跨端上下文验证（通过Session ID共享云端记忆）
    - 完成 Web 监控端（WSS实时日志）
    - 进行弱网重连测试、TLS 握手内存调优与 FreeRTOS 系统级压测。

## 6. 风险、依赖与后续迭代

- **核心风险：**
    - **API 学习曲线与内存管理：** 从 Arduino 转向纯 ESP-IDF C 开发，指针与 FreeRTOS 内存泄漏（Memory Leak）风险急剧升高。
    - **公网加密开销：** 开启 TLS/SSL 后，网络连接握手会瞬间消耗大量 RAM，需在 `menuconfig` 中将 MbedTLS 内存分配强制指定到 PSRAM，否则极易导致设备重启。
    - **UI 线程安全：** LVGL 在多任务环境下的线程安全（必须严格使用 Mutex 锁定 UI 刷新）。
- **依赖项：** 稳定公网服务器及域名、ESP-IDF v5.x 环境、大模型 API Key（托管于 NewAPI）。
- **后续迭代建议：** 引入离线唤醒词引擎（Espressif Wake Word Engine）替代 VAD，降低无效网络请求。
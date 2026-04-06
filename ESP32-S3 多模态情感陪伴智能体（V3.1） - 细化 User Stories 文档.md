# **ESP32-S3 多模态情感陪伴智能体（V3.1） - 细化 User Stories 文档**  

**Epic ID**：EPIC-002  
**文档版本**：V3.1（基于2026.04最新联网搜索细化）  
**生成日期**：2026年4月  
**角色**：Scrum Master（xiapiya 项目）  
**状态**：已细化 & 待Sprint规划  
**目标**：将Epic拆分为可独立开发、验收的User Stories（每条Story包含清晰的As a... I want... so that... + 验收标准 + 技术任务 + 复用点 + 风险）。  
**搜索依据**（2026.04最新）：  

- EMQX官方仓库 `emqx/esp32-mcp-mqtt-tutorial`（blog_4语音 + blog_6视觉，已更新至2025.08，支持ESP32-S3、INMP441/MAX98357A、OV2640、MCP over MQTT）。  
- LVGL v9.6 + TFT_eSPI最佳实践（PSRAM八线模式、IRAM加速、DMA传输、CPU 240MHz）。  
- DashScope Qwen-VL OpenAI兼容端点（`/compatible-mode/v1`，推荐 `qwen-vl-max` / `qwen-vl-plus`）。  
- MQTT主题最佳实践（分层设计、无前导通配符）。  
- Android/Kotlin MQTT客户端（HiveMQ/EMQX兼容，Foreground Service保活）。  

---

### **Feature 1：硬件底座与感知执行层（ESP32-S3）**  
**优先级**：P0（Phase 1）  
**估算**：8人天  

**User Story 1.1**  
As 硬件开发者，I want 通过I2S完成INMP441录音 + MAX98357A + 3W扬声器播放，so that 支持VAD能量触发语音流。  
**验收标准**：上电后VAD检测>阈值自动录音，播放MP3无卡顿；录音格式16kHz/16bit。  
**技术任务**：ESP-IDF/Arduino + I2S驱动；复用官方blog_4音频代码。  
**复用点**：emqx/esp32-mcp-mqtt-tutorial/samples/blog_4。  
**风险**：I2S引脚冲突（需与SPI TFT/摄像头DVP协调）。

**User Story 1.2**  
As 硬件开发者，I want 通过SPI驱动TFT屏幕 + LVGL v9.6显示动态UI（表情、字幕、状态栏），so that 提供现代化视觉反馈。  
**验收标准**：待机呼吸动画流畅（≥30FPS），支持表情切换 + 滚动字幕；使用PSRAM全帧缓冲。  
**技术任务**：TFT_eSPI + LVGL v9.6；启用`CONFIG_SPIRAM_MODE_OCT=y`、`CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y`、`CPU 240MHz`。  
**复用点**：LVGL官方ESP32-S3 TFT_eSPI示例 + Seeed XIAO ESP32-S3 Round Display模板。  
**风险**：内存不足（PSRAM必须≥8MB，推荐ESP32-S3-N16R8）。

**User Story 1.3**  
As 硬件开发者，I want OV2640摄像头抓拍JPEG并HTTP上传，so that 支持“看看我”视觉触发。  
**验收标准**：语音/远程指令触发后<2s完成抓拍+上传（JPEG压缩<300KB）。  
**技术任务**：DVP驱动 + CameraWebServer示例优化。  
**复用点**：官方blog_6摄像头代码。  

**User Story 1.4**  
As 硬件开发者，I want 全生命周期状态机（待机/录音/拍照/推理/播放），so that UI动效与硬件动作同步。  
**验收标准**：拍照时闪光+掩盖动画；推理中“神经元链接中...”动画无卡顿。  

---

### **Feature 2：交互触发与通信链路（双通道）**  
**优先级**：P0（Phase 1-2）  
**估算**：6人天  

**User Story 2.1**  
As 系统，I want 默认VAD触发语音流 + 特定指令/App指令触发视觉抓拍，so that 实现混合模态输入。  
**验收标准**：语音优先，App“远程拍照”指令优先级最高；3s防冲撞冷却。  

**User Story 2.2**  
As 系统，I want 轻数据（控制、音频、UI状态）走MQTT，重数据（JPEG）走HTTP POST，so that 保障低延迟与稳定性。  
**验收标准**：MQTT QoS 1/2，HTTP上传成功率>99%；无Broker过载。  
**技术任务**：主题规范（参考EMQX最佳实践）：  
`emqx/{product}/esp32/{device_id}/audio`、`emqx/{product}/esp32/{device_id}/ui`、`emqx/{product}/esp32/{device_id}/control` 等。  
**复用点**：官方MCP over MQTT主题 + blog_6图像上传逻辑。  

---

### **Feature 3：云端大脑（Python/FastAPI逻辑中枢）**  
**优先级**：P0（Phase 3）  
**估算**：10人天  

**User Story 3.1**  
As AI大脑，I want 路由音频Webhook与图像HTTP POST，融合Qwen-VL（带图）或Omni（纯语音），生成≤20字回复 + 情绪标签，so that 提供情感化回应。  
**验收标准**：带图场景调用`qwen-vl-max`（OpenAI兼容端点 `/compatible-mode/v1/chat/completions`）；纯语音兜底Omni；回复格式JSON `{ "text": "...", "emotion": "happy" }`。  
**技术任务**：FastAPI异步 + DashScope SDK；提示词严格限制字数 + 情绪提取。  
**复用点**：官方blog_6 Qwen-VL调用代码（已支持OpenAI兼容）。  

**User Story 3.2**  
As AI大脑，I want 合成Cherry音色MP3并通过MQTT下发 + 实时推送UI状态，so that 掩盖推理延迟。  
**验收标准**：推理中立即推送`ui/status`状态节点；TTS MP3<5s生成。  
**技术任务**：lameenc转码 + MQTT publish。  

---

### **Feature 4：屏幕UI与视觉表达（TFT + LVGL）**  
**优先级**：P1（Phase 2）  
**估算**：7人天  

**User Story 4.1**  
As 用户，I want 在TFT屏幕看到现代化UI（表情区 + 动态字幕 + WiFi/MQTT状态栏），so that 获得生动陪伴感。  
**验收标准**：5种情绪表情（happy/sad/comfort等）一一映射；字幕支持滚动+淡入淡出。  
**技术任务**：LVGL v9.6状态机 + 动画API；表情图片预加载到PSRAM。  
**复用点**：LVGL ESP32-S3官方示例 + TFT_eSPI。  

**User Story 4.2**  
As 用户，I want 推理延迟被“神经元链接中...”动画掩盖，回复时表情+字幕+音频完全同步，so that 体验流畅无等待。  
**验收标准**：动画覆盖全屏3-8s；同步率100%。  

---

### **Feature 5：多端协同（Web监控 + Android伴侣）**  
**优先级**：P1（Phase 3-4）  
**估算**：12人天  

**User Story 5.1**  
As 管理员，I want 在Web看板实时查看设备动作流、抓拍图像、耗时统计，so that 远程监控。  
**验收标准**：MQTT.js订阅所有主题；图像实时展示；耗时图表（WebSocket）。  
**复用点**：官方MQTT主题 + 现有Web模板。  

**User Story 5.2**  
As 用户，I want 在Android App进行远程语音/拍照、查看历史记录（Room本地化）、控制“静音硬件”开关，so that 多端无缝协同。  
**验收标准**：Foreground Service + Notification保活；Room本地持久化历史；远程拍照指令即时生效。  
**技术任务**：Kotlin + MQTT客户端（Paho或HiveMQ兼容EMQX）；Room + WorkManager。  
**复用点**：EMQX MQTT最佳实践示例。  

---

### **全局API与通信协议规范（已更新）**  
- **MQTT**：采用分层主题 `{product}/esp32/{device_id}/{type}`（避免#/+前导）。  
- **HTTP**：`POST /upload/image`（JPEG）。  
- **Qwen-VL**：使用最新兼容端点 `https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions`，模型优先`qwen-vl-max`。  

### **里程碑与开发路线图（保持不变）**  
- Phase 1（第1周）：硬件 + MQTT/HTTP基础 + Fork官方blog_4+blog_6。  
- Phase 2（第2-3周）：TFT+LVGL UI + 单模态语音闭环。  
- Phase 3（第4-5周）：OV2640 + Qwen-VL视觉 + Web监控。  
- Phase 4（第6-8周）：Android App + 历史持久化 + 多设备防冲突。  

**总工时估算**：Fork官方后仍约8-10人周（1人全栈可完成）。  

**风险更新（2026.04）**  
- LVGL性能：已确认PSRAM + IRAM优化可达30FPS+。  
- Qwen-VL费用：使用`qwen-vl-plus`兜底 + 图像缓存。  
- Android保活：Foreground Service + Notification已成熟。  

**Epic完成定义**（不变）：硬件端“看”到用户并TFT生动表达；App远程视觉触发；全链路实时监控；多模态温度陪伴体验达成。  


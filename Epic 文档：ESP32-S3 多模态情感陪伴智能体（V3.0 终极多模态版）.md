# Epic 文档：ESP32-S3 多模态情感陪伴智能体（V3.0 终极多模态版）

**Epic ID**：EPIC-002  
**所属系列**：从0到1打造“情感陪伴智能体”系列（第4-6篇综合落地指南）  
**文档版本**：V3.0（基于更新PRD + 2026年4月最新联网评估）  
**产品负责人**：xiapiya  
**状态**：已确认 & 已细化（2026.04）  
**目标交付**：一款具备“听、说、看、表达”四维能力的桌面具身情感陪伴智能体，实现语音+视觉触发、TFT屏幕丰富UI、多端（Web/App）实时协同。  
**预计价值**：从纯语音升级为多模态，差异化极强；用户可通过语音/远程指令“让AI看见我”，提供更具温度的陪伴体验。

## 1. Epic 背景与核心目标
- **业务价值**：基于ESP32-S3强大算力（16MB Flash + 8MB PSRAM），结合Qwen-VL + Omni多模态大模型，打造“会听、会说、会看、会表达”的桌面宠物级智能终端，解决单一语音陪伴的局限性。
- **核心架构**：
  - **感知执行层**：ESP32-S3（I2S音频 + OV2640摄像头 + SPI TFT屏幕 + LVGL UI）。
  - **通信中枢**：EMQX MQTT（轻量控制/音频） + HTTP（大图上传）。
  - **逻辑大脑**：Python/FastAPI（路由分发 + VLM/LLM融合 + TTS）。
  - **监控/伴侣端**：Web只读看板 + Android聊天界面。
- **非功能需求**：低延迟（视觉触发<8s）、稳定（双通道解耦）、轻量化、成本可控（硬件<150元/套）、易扩展。
- **成功标准**：Phase 2跑通单模态闭环；Phase 4完成多模态全链路，用户可在硬件端看到动态表情+字幕，手机远程拍照并收到视觉增强回复。

## 2. 类似项目参考（2026年4月最新联网搜索结果 - 避免重复造轮子）
已搜索EMQX官网、GitHub、LVGL论坛、Reddit等。**官方系列是最匹配的现成轮子**（已覆盖第4篇语音 + 第6篇视觉）。

| 排名 | 项目/仓库                                                    | 相似度       | 核心匹配点                                                   | 与本Epic差异                                                 | 推荐行动                                                     |
| ---- | ------------------------------------------------------------ | ------------ | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| 1    | **EMQX官方 esp32-mcp-mqtt-tutorial (samples/blog_4 + blog_6)** | ★★★★★ (95%+) | ESP32-S3 + INMP441/MAX98357A + OV2640 + Qwen-VL + MCP工具调用 + 图像HTTP上传 + MQTT控制 + 情感回复 | 官方用IIC LCD（无LVGL/TFT丰富UI）；MCP重度工具调用；无Android/Web完整多端 | **立即Fork**：https://github.com/mqtt-ai/esp32-mcp-mqtt-tutorial<br>直接用blog_4（语音）+ blog_6（视觉）作为Phase 1-3基础 |
| 2    | LVGL官方 ESP32-S3 TFT_eSPI 示例 + TFT_eSPI库                 | ★★★★         | ESP32-S3 + SPI TFT + LVGL动画/状态机                         | 无AI/摄像头/MQTT                                             | 直接复用LVGL移植代码，提升屏幕UI                             |
| 3    | 其他ESP32-S3多模态项目（T-Display-S3-MQTT、WT32-SC01）       | ★★★          | SPI TFT + LVGL + MQTT                                        | 无Qwen-VL视觉、无FastAPI大脑                                 | 借鉴TFT引脚配置和LVGL状态机                                  |
| 4    | Qwen-VL + ESP32-CAM HTTP上传示例                             | ★★★          | OV2640 JPEG + HTTP POST到DashScope                           | 无完整多端联动                                               | 复用图像上传+ Qwen-VL OpenAI兼容调用                         |

**结论**：**直接Fork官方blog_4 + blog_6** 可节省70%工作量。只需在其基础上替换/增强TFT+LVGL UI、添加Android/Web端，即可完美对齐PRD。

## 3. 需求拆分（Feature / User Story 层级）
本Epic拆分为**5个核心Feature**（对应PRD模块 + 细化），每个包含用户故事、技术要求、验收标准及复用点。

### Feature 1：硬件底座与感知执行层（ESP32-S3）
**优先级**：P0（Phase 1）  
**用户故事**：
- As 硬件，我能通过I2S完成音频录制（INMP441）和播放（MAX98357A + 3W扬声器）。
- As 硬件，我能通过SPI驱动TFT屏幕 + LVGL显示动态UI（表情、字幕、状态栏）。
- As 硬件，用户说“看看我这件衣服”或远程指令时，OV2640抓拍JPEG并上传。
- As 硬件，全生命周期状态机支持待机微动效、拍照闪光、推理掩盖动画。

**技术要求**：ESP-IDF/Arduino、TFT_eSPI + LVGL、OV2640 DVP驱动、VAD能量触发  
**复用**：官方blog_4（音频）+ blog_6（摄像头） + LVGL ESP32示例  
**验收标准**：硬件上电后进入待机呼吸动画；语音/指令触发拍照并上传成功。

### Feature 2：交互触发与通信链路（双通道）
**优先级**：P0（Phase 1-2）  
**用户故事**：
- As 系统，默认VAD触发语音流；特定指令或App/Web指令触发视觉抓拍。
- As 系统，轻数据（控制、音频、状态）走MQTT，重数据（JPEG图像）走HTTP POST，保障稳定与实时性。

**技术要求**：MQTT主题规范 + HTTP路由；防冲撞（3s冷却）  
**复用**：官方MCP over MQTT主题 + blog_6图像上传逻辑  
**验收标准**：语音/视觉混合场景下无丢帧、无Broker压力过载。

### Feature 3：云端大脑（Python/FastAPI逻辑中枢）
**优先级**：P0（Phase 3）  
**用户故事**：
- As AI大脑，我能路由音频（Webhook）与图像（HTTP POST），融合Qwen-VL（带图）或Omni（纯语音），生成≤20字回复 + 情绪标签。
- As AI大脑，我能合成Cherry音色MP3并通过MQTT下发；推理中实时推送UI状态节点掩盖延迟。

**技术要求**：FastAPI异步、DashScope SDK（Qwen-VL OpenAI兼容）、lameenc转码  
**复用**：官方blog_6 Qwen-VL调用代码  
**验收标准**：带图/不带图场景均返回正确情感回复 + 屏幕同步更新。

### Feature 4：屏幕UI与视觉表达（TFT + LVGL）
**优先级**：P1（Phase 2）  
**用户故事**：
- As 用户，我能在TFT屏幕看到现代化UI：表情区（happy/sad/comfort）、动态字幕、状态栏（WiFi/MQTT图标）。
- As 用户，推理延迟被“神经元链接中...”动画掩盖，回复时表情+字幕+音频同步。

**技术要求**：LVGL v9+、TFT_eSPI、状态机映射  
**复用**：LVGL ESP32-S3官方示例 + TFT_eSPI库  
**验收标准**：全生命周期动效流畅，表情与情绪标签一一对应。

### Feature 5：多端协同（Web监控 + Android伴侣）
**优先级**：P1（Phase 3-4）  
**用户故事**：
- As 管理员，我能在Web看板实时查看设备动作流、抓拍图像、耗时统计。
- As 用户，我能在Android App进行远程语音/拍照、查看历史记录（Room本地化）、控制“静音硬件”开关。

**技术要求**：HTML/JS + MQTT.js（Web）；Kotlin + HiveMQ + Room + Foreground Service（Android）  
**复用**：官方MQTT主题 + 现有Web模板  
**验收标准**：多端联动无冲突；App后台保活 + 远程拍照生效。

## 4. API 与 通信协议规范（全局共享）
- **MQTT**（轻数据）：`emqx/esp32/audio`、`emqx/esp32/playaudio`、`emqx/esp32/ui`（JSON: text/emotion/status）、`emqx/system/logs`、`emqx/control/*`。
- **HTTP**（重数据）：POST `/upload/image`（JPEG）→ FastAPI。
- **Qwen-VL提示词**：严格≤20字 + 情绪标签提取 + Cherry TTS。

## 5. 里程碑与开发路线图（调整版 - 8周可落地）
- **Phase 1**（第1周）：硬件拼装 + MQTT/HTTP基础 + Fork官方blog_4+blog_6。
- **Phase 2**（第2-3周）：TFT+LVGL UI + 单模态语音闭环。
- **Phase 3**（第4-5周）：OV2640 + Qwen-VL视觉 + Web监控。
- **Phase 4**（第6-8周）：Android App + 历史持久化 + 多设备防冲突 + 商业级闭环。

**总预计工时**：Fork官方代码后约 **8-10 人周**（1人全栈可完成）。

## 6. 风险、依赖与后续迭代
- **风险**：
  - LVGL在SPI TFT上的内存/性能（PSRAM已足够，用ESP32-S3-N16R8完美）。
  - 图像上传延迟/大小（JPEG压缩 + HTTP优化）。
  - Qwen-VL API费用与速率（加缓存+兜底）。
  - Android后台保活（Foreground Service + Notification）。
- **依赖**：EMQX Serverless、DashScope API Key、TFT_eSPI/LVGL库。
- **后续迭代建议**（下一Epic）：
  - 情绪记忆 + 人格化（官方Part 5）。
  - 多设备路由面板。
  - 本地VLM轻量化（降低云端依赖）。

**Epic 完成定义**：硬件端能“看”到用户并在TFT上生动表达；手机App可远程触发视觉陪伴；全链路状态实时监控；用户获得极具温度的多模态情感交互体验。


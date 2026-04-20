# xiapiya 情感陪伴智能体 - 项目总结文档

**项目代号**: xiapiya
**文档版本**: V1.0 (MVP极简版)
**最后更新**: 2026-04-20
**项目状态**: 开发中 (核心框架已完成，音频链路待完善)

---

## 1. 项目概述

### 1.1 核心定位
打造一款基于 ESP32-S3 的桌面情感陪伴智能体，具备**听、说、表达**三维能力（暂不包含视觉），通过公网云端服务器调度通义千问大模型实现多模态情感交互。

### 1.2 核心价值
- **边缘+云端协同**: ESP32-S3边缘音频处理 + 云端大模型推理
- **跨端记忆同步**: 桌面硬件与移动端共享统一上下文
- **极简交互体验**: 纯音频对话 + 极简几何UI反馈
- **生产级架构**: 容器化部署 + 端到端TLS加密

---

## 2. 技术架构

### 2.1 系统架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    公网 (Internet)                           │
└────────────┬────────────────────────────────────────┬────────┘
             │                                        │
    ┌────────▼────────┐                      ┌───────▼──────┐
    │  ESP32-S3设备    │                      │  Android App  │
    │  (MQTTS+HTTPS)  │                      │  Web Monitor  │
    └────────┬────────┘                      └───────┬──────┘
             │                                        │
             │          TLS/SSL加密传输                │
             │                                        │
    ┌────────▼────────────────────────────────────────▼────────┐
    │              云服务器 (Linux + 1Panel)                    │
    │  ┌──────────────────────────────────────────────────┐   │
    │  │               Nginx (反向代理)                     │   │
    │  └──┬──────────────────────────────┬────────────────┘   │
    │     │                              │                     │
    │  ┌──▼──────────┐           ┌──────▼──────────┐         │
    │  │ EMQX (MQTTS) │           │ FastAPI (HTTPS) │         │
    │  │  Docker容器   │◄─────────┤   Docker容器     │         │
    │  └──────────────┘           └────────┬────────┘         │
    │                                      │                   │
    │                             ┌────────▼────────┐         │
    │                             │ NewAPI (模型网关)│         │
    │                             │   Docker容器     │         │
    │                             └────────┬────────┘         │
    │                                      │ 内网              │
    │                             ┌────────▼────────┐         │
    │                             │  Qwen大模型API  │         │
    │                             │  (阿里云DashScope)│         │
    │                             └─────────────────┘         │
    └─────────────────────────────────────────────────────────┘
```

### 2.2 核心组件

#### 硬件端 (ESP32-S3)
- **主控芯片**: ESP32-S3-N16R8 (16MB Flash + 8MB PSRAM)
- **开发框架**: 纯 ESP-IDF v5.x (工业级FreeRTOS调度)
- **音频系统**:
  - INMP441 麦克风 (I2S录音)
  - MAX98357A 功放 (I2S播放)
  - 3W扬声器
- **显示系统**: 2.2寸 ILI9341 TFT屏幕 (SPI) + LVGL极简UI
- **通信协议**: MQTTS (轻数据) + HTTPS (重数据)

#### 云端服务器
- **部署方式**: 1Panel容器化管理
- **核心服务**:
  - **EMQX**: MQTT消息代理 (TLS加密)
  - **FastAPI**: Python后端API (音频/图片处理)
  - **NewAPI**: 统一模型网关 (管理API Key和并发)
  - **Nginx**: 反向代理 (SSL证书管理)
- **大模型**: 通义千问 Qwen-Omni (音频对话)

#### 移动端 (Android V4.2)
- **技术栈**: Kotlin + Retrofit + OkHttp-SSE + Room + MVVM
- **核心功能**:
  - 纯文本对话 (流式渲染)
  - 跨端记忆同步 (共享云端上下文)
  - 本地持久化 (Room数据库)
  - 极简解耦设计 (不控制硬件)
- **身份识别**: 硬编码Session ID `xiapiya_master_01`

#### Web监控端
- **技术栈**: HTML/JS + WebSocket Secure (WSS)
- **功能**: 实时查看云端处理链路日志
- **权限**: 只读模式 (不下发控制指令)

---

## 3. 核心业务流程

### 3.1 音频对话流程

```
用户说话
    ↓
ESP32 VAD唤醒 → 屏幕显示"..." (倾听态)
    ↓
I2S录音 (16kHz PCM)
    ↓
MQTTS上传音频数据 → 屏幕闪烁"..." (思考态)
    ↓
云端FastAPI接收 → NewAPI → Qwen-Omni推理
    ↓
生成回复文本 + 情绪标签
    ↓
TTS合成MP3音频 (Cherry音色)
    ↓
MQTTS下发音频 + UI状态指令
    ↓
ESP32接收 → 屏幕显示呼吸圆 (说话态) + I2S播放音频
    ↓
播放完成 → 屏幕回归待机态
```

### 3.2 Android文本对话流程

```
用户在手机输入文本
    ↓
HTTP POST /api/v1/chat/stream (session_id: xiapiya_master_01)
    ↓
云端FastAPI提取session_id对应的完整上下文
    ↓
包含ESP32的语音对话历史 + Android的文本历史
    ↓
调用Qwen大模型生成回复
    ↓
SSE流式返回 (逐字渲染)
    ↓
Room数据库保存对话记录
```

### 3.3 极简UI状态机

ESP32 TFT屏幕显示5种状态：

| 状态 | 显示内容 | 触发条件 |
|-----|---------|---------|
| **待机态** | 空白/极暗背光 | 无交互，省电 |
| **倾听态** | 静态三个点 `...` | VAD唤醒录音 |
| **思考态** | 闪烁三个点 `...` | 等待云端推理 |
| **说话态** | 呼吸圆动画 | 播放TTS音频 |
| **错误态** | 红色图标 | 网络断开 |

---

## 4. 开发进度

### 4.1 已完成 ✅

#### Android移动端 (100%)
- ✅ MVVM项目框架
- ✅ Retrofit + OkHttp HTTP客户端
- ✅ SSE流式渲染客户端
- ✅ Room数据库 (ChatMessage/ChatDao/AppDatabase)
- ✅ Repository模式 (ChatRepository)
- ✅ ViewModel + LiveData
- ✅ RecyclerView聊天界面
- ✅ 左右气泡布局 (用户蓝色/AI灰色)
- ✅ Material Design 3主题
- ✅ 硬编码Session ID管理
- ✅ 完整README文档

**文件统计**: 34个文件，2435行代码

#### 后端FastAPI (MVP版本 70%)
- ✅ FastAPI项目框架
- ✅ POST /api/v1/vision/upload 接口 (图片上传)
- ✅ NewAPI客户端集成
- ✅ Qwen-VL视觉识别
- ✅ 10秒超时+兜底机制
- ✅ 结构化JSON日志
- ✅ 自定义异常处理
- ✅ Pydantic数据模型
- ✅ Black/Ruff/MyPy代码质量检查通过

**文件统计**: 20个文件，1500+行代码

#### 云端基础设施 (90%)
- ✅ 1Panel容器化管理平台
- ✅ EMQX MQTT代理 (TLS加密)
- ✅ NewAPI模型网关部署
- ✅ Nginx反向代理配置
- ✅ SSL证书配置
- ✅ 域名解析

#### ESP32硬件端 (基础框架 60%)
- ✅ ESP-IDF项目初始化
- ✅ FreeRTOS任务调度设计
- ✅ I2S音频录播驱动 (INMP441 + MAX98357A)
- ✅ MQTTS连接 (mbedTLS加密)
- ✅ TFT屏幕驱动 (ILI9341 + LVGL)
- ✅ 硬件引脚配置完成

### 4.2 待完成 ⏳

#### 后端音频链路 (P0 - 紧急)
- [ ] **TTS音频合成服务** (调用阿里云TTS或讯飞TTS)
  - Cherry音色配置
  - MP3格式，32kbps码率
  - 异步调用，3秒超时
- [ ] **MQTT音频下发模块**
  - 连接EMQX MQTTS
  - 发布到 `aifly/esp32/{device_id}/playaudio` 主题
  - Base64编码或Raw二进制传输
- [ ] **POST /api/v1/chat/text 接口** (Android文本对话)
  - 同步响应模式
- [ ] **POST /api/v1/chat/stream 接口** (Android流式对话)
  - SSE Server-Sent Events实现
  - 逐字流式返回
- [ ] **Session ID上下文管理**
  - Redis/MySQL存储对话历史
  - 实现跨端记忆索引
- [ ] **JWT鉴权中间件** (可选，暂时跳过)

#### ESP32硬件端 (P1)
- [ ] LVGL极简UI动画实现
  - 呼吸圆动画 (lv_anim_t)
  - 状态机切换逻辑
  - 顶部WiFi/MQTT状态图标
- [ ] MQTT音频接收与播放
  - 订阅 `playaudio` 主题
  - Base64解码
  - I2S队列播放
- [ ] VAD语音活动检测
  - 本地VAD判断
  - 触发录音上传
- [ ] 完整的状态机实现
  - IDLE → RECORDING → CLOUD_SYNC → PLAYING → IDLE

#### Web监控端 (P2 - 可选)
- [ ] WSS连接EMQX
- [ ] 实时日志流展示
- [ ] Chart.js性能图表

---

## 5. 核心剩余工作分析

根据项目进度，**核心剩余工作集中在后端音频链路**，具体包括：

### 5.1 TTS音频合成 (预计2天)

**技术选型**:
- 阿里云TTS (cosyvoice-v1模型，Cherry音色) - 推荐
- 讯飞TTS (备选)

**实现要点**:
```python
# app/services/tts.py
from alibabacloud_nls_sdk import TTS

async def synthesize_speech(text: str) -> bytes:
    """
    将文本转换为MP3音频
    - 音色: Cherry
    - 格式: MP3, 32kbps
    - 超时: 3秒
    """
    # 调用阿里云TTS API
    audio_data = await tts_client.synthesize(
        text=text,
        voice="cherry",
        format="mp3",
        sample_rate=16000
    )
    return audio_data
```

### 5.2 MQTT音频下发 (预计1天)

**实现要点**:
```python
# app/services/mqtt_client.py
import asyncio_mqtt as aiomqtt

async def publish_audio(device_id: str, audio_data: bytes):
    """
    下发音频到ESP32
    - 主题: aifly/esp32/{device_id}/playaudio
    - 格式: Base64编码 (MQTT协议限制)
    """
    async with aiomqtt.Client("emqx.yourdomain.com", 8883, tls_context=ssl_context) as client:
        await client.publish(
            f"aifly/esp32/{device_id}/playaudio",
            base64.b64encode(audio_data)
        )
```

### 5.3 Android文本对话接口 (预计2天)

**接口1: 同步响应**
```python
# app/api/v1/chat.py
@router.post("/text")
async def chat_text(request: ChatRequest):
    """
    同步文本对话接口
    - 提取session_id对应的完整历史
    - 调用Qwen大模型
    - 返回完整回复
    """
    # 1. 从Redis/MySQL获取历史
    history = await context_manager.get_history(request.session_id)

    # 2. 调用NewAPI/Qwen
    reply = await newapi_client.call_qwen(
        messages=[*history, {"role": "user", "content": request.content}]
    )

    # 3. 保存历史
    await context_manager.save_message(request.session_id, request.content, reply)

    return {"reply": reply, "emotion": "neutral", "timestamp": time.time()}
```

**接口2: SSE流式响应**
```python
@router.post("/stream")
async def chat_stream(request: ChatRequest):
    """
    SSE流式对话接口
    - 逐字返回AI回复
    - Content-Type: text/event-stream
    """
    async def event_generator():
        history = await context_manager.get_history(request.session_id)

        async for chunk in newapi_client.stream_qwen(messages=[...]):
            yield f"data: {chunk}\n\n"

        yield "event: done\ndata: {}\n\n"

    return StreamingResponse(event_generator(), media_type="text/event-stream")
```

### 5.4 Session上下文管理 (预计2天)

**数据结构设计**:
```python
# Redis Schema
# Key: session:{session_id}:history
# Value: JSON Array
[
    {"role": "user", "content": "主人在吗", "source": "esp32", "timestamp": 1713514200},
    {"role": "assistant", "content": "在的主人", "emotion": "happy", "timestamp": 1713514205},
    {"role": "user", "content": "今天天气怎么样", "source": "android", "timestamp": 1713514300}
]
```

**实现要点**:
```python
# app/services/context_manager.py
class ContextManager:
    async def get_history(self, session_id: str, limit: int = 20):
        """获取最近N条历史"""
        key = f"session:{session_id}:history"
        return await redis.lrange(key, -limit, -1)

    async def save_message(self, session_id: str, user_msg: str, ai_reply: str, source: str):
        """保存对话"""
        key = f"session:{session_id}:history"
        await redis.rpush(key, json.dumps({
            "role": "user",
            "content": user_msg,
            "source": source,
            "timestamp": time.time()
        }))
        await redis.rpush(key, json.dumps({
            "role": "assistant",
            "content": ai_reply,
            "timestamp": time.time()
        }))
        await redis.expire(key, 86400 * 30)  # 保留30天
```

---

## 6. 工作量评估

### 6.1 后端剩余工作

| 任务 | 优先级 | 预计工时 | 难度 |
|-----|--------|---------|------|
| TTS音频合成 | P0 | 2天 | 中 |
| MQTT音频下发 | P0 | 1天 | 低 |
| /chat/text 接口 | P0 | 1天 | 低 |
| /chat/stream SSE接口 | P0 | 1天 | 中 |
| Session上下文管理 | P0 | 2天 | 中 |
| JWT鉴权 (可选) | P2 | 1天 | 低 |
| **总计** | - | **7-8天** | - |

### 6.2 ESP32剩余工作

| 任务 | 优先级 | 预计工时 | 难度 |
|-----|--------|---------|------|
| LVGL极简UI动画 | P1 | 3天 | 中 |
| MQTT音频接收播放 | P1 | 2天 | 中 |
| VAD唤醒逻辑 | P1 | 2天 | 中 |
| 完整状态机集成 | P1 | 2天 | 中 |
| **总计** | - | **9天** | - |

### 6.3 总体时间线

**当前状态**: 已完成60%基础框架

**剩余核心工作**: 后端音频链路 (P0) + ESP32状态机 (P1)

**预计完成时间**:
- **最快路径**: 10天 (仅完成P0后端音频 + 基础ESP32播放)
- **完整版本**: 15-20天 (包含LVGL UI + Web监控)

---

## 7. 技术亮点

### 7.1 架构设计
- **工业级底层**: 纯ESP-IDF开发，FreeRTOS多任务调度
- **安全加密**: 端到端TLS/SSL加密 (MQTTS + HTTPS)
- **容器化部署**: 1Panel统一管理，一键部署
- **内网模型网关**: NewAPI统一管理API Key，防泄漏

### 7.2 用户体验
- **跨端记忆**: Android与ESP32共享云端上下文
- **流式渲染**: SSE逐字显示，实时感强
- **极简UI**: 几何图形状态机，减少视觉负担
- **兜底机制**: 网络超时自动触发预置回复

### 7.3 工程实践
- **Clean Architecture**: MVVM分层，Repository模式
- **代码质量**: Black/Ruff/MyPy全覆盖
- **结构化日志**: JSON格式，便于ELK集成
- **异步优先**: Python async/await + Kotlin协程

---

## 8. 项目文件结构

```
AI_FLY/
├── android/                            # Android移动端
│   └── xiapiya-companion/
│       └── app/src/main/java/
│           ├── data/                   # 数据层 (Room+Retrofit)
│           ├── presentation/           # UI层 (ViewModel+Activity)
│           └── util/                   # 工具类 (Constants)
│
├── backend/                            # FastAPI云端后端
│   ├── app/
│   │   ├── main.py                    # 应用入口
│   │   ├── config.py                  # 配置管理
│   │   ├── api/v1/                    # API路由 (vision.py)
│   │   ├── services/                  # 服务层 (newapi/tts/mqtt)
│   │   ├── models/                    # 数据模型
│   │   └── core/                      # 核心模块 (日志/异常)
│   ├── requirements.txt
│   └── README.md
│
├── esp32/                              # ESP32硬件端
│   └── esp32-s3-mqtts-audio/
│       ├── main/                      # 主程序
│       │   ├── main.c                 # 入口文件
│       │   ├── audio_record.c         # 音频录制
│       │   ├── audio_play.c           # 音频播放
│       │   ├── mqtt_client.c          # MQTT通信
│       │   ├── tft_display.c          # TFT屏幕驱动
│       │   └── state_machine.c        # 状态机
│       └── CMakeLists.txt
│
├── .trellis/                           # AI开发工作流
│   ├── spec/                          # 开发规范
│   │   ├── frontend/                  # Android规范
│   │   ├── backend/                   # FastAPI规范
│   │   └── guides/                    # 最佳实践
│   ├── tasks/                         # 任务跟踪
│   │   ├── 04-16-backend-vision-api/
│   │   ├── 04-19-android-text-client/
│   │   └── 04-16-esp32-tft-camera/
│   └── workspace/                     # 工作记录
│       └── xiapiya/
│           └── journal-1.md
│
├── Epic 文档：ESP32-S3 多模态情感陪伴智能体（V4.0 云端架构演进版）.md
├── 产品需求文档 (PRD)：ESP32-S3 多模态情感陪伴智能体.md
├── android_v4.2_migration_summary.md
└── PROJECT_SUMMARY.md                  # 本文档
```

---

## 9. 快速开始

### 9.1 后端部署

```bash
# 1. 安装依赖
cd backend
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# 2. 配置环境变量
cp .env.example .env
# 编辑.env，配置NEWAPI_API_KEY、TTS_API_KEY等

# 3. 启动服务
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000

# 4. 访问API文档
# http://localhost:8000/docs
```

### 9.2 Android开发

```bash
# 1. 打开Android Studio
# 2. 导入项目: android/xiapiya-companion
# 3. 修改Constants.kt中的API_BASE_URL
# 4. 运行到真机或模拟器
```

### 9.3 ESP32烧录

```bash
# 1. 安装ESP-IDF v5.x
# 2. 进入项目目录
cd esp32/esp32-s3-mqtts-audio

# 3. 配置项目
idf.py menuconfig
# - 配置WiFi SSID/Password
# - 配置MQTT Broker地址

# 4. 编译烧录
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## 10. 常见问题

### 10.1 NewAPI连接失败
**解决方案**: 检查NewAPI服务是否运行
```bash
curl http://localhost:3000/v1/models
```

### 10.2 Android无法连接后端
**解决方案**:
1. 检查API_BASE_URL配置是否正确
2. 确保后端服务已启动
3. 检查防火墙规则

### 10.3 ESP32 MQTT断连
**解决方案**:
1. 检查WiFi信号强度
2. 检查EMQX服务状态
3. 检查mbedTLS证书是否正确

---

## 11. 下一步计划

### 11.1 本周重点 (P0)
1. 完成后端TTS音频合成服务
2. 实现MQTT音频下发模块
3. 开发Android文本对话接口 (/chat/text + /chat/stream)
4. 实现Session上下文管理 (Redis)

### 11.2 下周计划 (P1)
1. ESP32 MQTT音频接收与I2S播放
2. LVGL极简UI动画实现
3. 完整状态机集成测试
4. 跨端记忆同步验证

### 11.3 未来扩展 (P2)
1. Web监控端 (WSS实时日志)
2. 离线唤醒词 (Espressif Wake Word Engine)
3. 多设备管理 (支持多个ESP32)
4. 语音克隆 (个性化TTS)

---

## 12. 相关文档

- **Epic文档**: `Epic 文档：ESP32-S3 多模态情感陪伴智能体（V4.0 云端架构演进版）.md`
- **产品PRD**: `产品需求文档 (PRD)：ESP32-S3 多模态情感陪伴智能体.md`
- **Android迁移总结**: `android_v4.2_migration_summary.md`
- **后端README**: `backend/README.md`
- **后端实现总结**: `backend/IMPLEMENTATION_SUMMARY.md`
- **Android任务PRD**: `.trellis/tasks/04-19-android-text-client/prd.md`
- **后端任务PRD**: `.trellis/tasks/04-16-backend-vision-api/prd.md`

---

## 13. 团队协作

**开发者**: xiapiya
**工作流**: Trellis AI辅助开发流程
**代码规范**:
- Backend: `.trellis/spec/backend/`
- Frontend: `.trellis/spec/frontend/`
- Guides: `.trellis/spec/guides/`

**任务管理**: `.trellis/tasks/`
**工作记录**: `.trellis/workspace/xiapiya/journal-1.md`

---

## 14. 总结

**项目当前状态**: 基础框架已完成60%，核心待完成工作是后端音频链路

**核心优势**:
- 工业级ESP-IDF架构
- 容器化云端部署
- 跨端记忆同步
- 极简用户体验

**剩余核心工作**: 后端TTS/MQTT音频 + Session管理 (预计7-8天)

**项目特色**: 真实物联网项目，非Demo，适合面试展示

---

**文档生成时间**: 2026-04-20
**最后更新**: 2026-04-20
**文档版本**: V1.0

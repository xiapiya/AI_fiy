# Directory Structure (V4.0 云端架构版)

> Python/FastAPI 项目目录组织规范
>
> **版本演进**: V4.0新增NewAPI网关集成、云端部署配置

---

## Overview

后端使用 **Python + FastAPI** 实现云端AI大脑,负责:
- 音频/图像数据处理
- 通过**NewAPI网关**调度大模型 (Qwen-VL, Qwen-Omni)
- MQTT消息路由与下发
- TTS音频生成与转码
- 公网HTTPS接口暴露

**核心设计原则**: 分层架构 + 依赖注入 + 异步优先 + 网关安全

---

## Directory Layout

```
backend/
├── app/
│   ├── __init__.py
│   ├── main.py              # FastAPI应用入口
│   ├── config.py            # 配置管理(环境变量、NewAPI端点、MQTT配置等)
│   ├── api/                 # API路由层 (V4.0规范化命名)
│   │   ├── __init__.py
│   │   └── v1/              # API版本管理
│   │       ├── __init__.py
│   │       ├── audio.py     # POST /api/v1/audio (EMQX Webhook)
│   │       └── vision.py    # POST /api/v1/vision/upload (HTTPS图像上传)
│   ├── services/            # 业务逻辑层
│   │   ├── __init__.py
│   │   ├── newapi.py        # NewAPI网关客户端 (V4.0新增)
│   │   ├── qwen_vl.py       # Qwen-VL多模态调用 (通过NewAPI)
│   │   ├── qwen_omni.py     # Qwen-Omni语音调用 (通过NewAPI)
│   │   ├── tts.py           # TTS合成 + MP3转码 (32kbps)
│   │   ├── audio_filter.py  # 静音/噪音前置检测
│   │   ├── mqtt.py          # MQTTS客户端 (V4.0启用TLS)
│   │   └── fallback.py      # 兜底音频管理 (V4.0新增)
│   ├── models/              # 数据模型(Pydantic)
│   │   ├── __init__.py
│   │   ├── requests.py      # 请求模型(AudioRequest, VisionRequest)
│   │   └── responses.py     # 响应模型(AIResponse, UIUpdate)
│   ├── core/                # 核心配置 (V4.0新增)
│   │   ├── __init__.py
│   │   ├── config.py        # 环境变量配置类
│   │   └── security.py      # JWT/Token验证
│   ├── utils/               # 工具函数
│   │   ├── __init__.py
│   │   ├── logger.py        # 结构化JSON日志
│   │   └── audio_utils.py   # 音频处理工具
│   └── exceptions/          # 自定义异常
│       ├── __init__.py
│       └── errors.py        # LLMAPIError, MQTTPublishError, NewAPIError等
├── tests/                   # 测试代码
│   ├── test_audio.py
│   ├── test_vision.py
│   ├── test_newapi.py
│   └── test_mqtt.py
├── deploy/                  # 部署配置 (V4.0新增)
│   ├── docker-compose.yml   # 1Panel部署配置
│   ├── nginx.conf           # Nginx SSL反向代理
│   └── supervisord.conf     # 进程管理
├── certs/                   # SSL证书 (V4.0新增)
│   ├── server.crt           # HTTPS服务器证书
│   ├── server.key           # 私钥
│   └── ca.pem               # MQTTS CA证书
├── requirements.txt         # 依赖列表
├── .env.example             # 环境变量示例
├── Dockerfile               # 容器化部署
└── README.md
```

---

## Module Organization

### 1. 分层原则

| 层级 | 职责 | 禁止事项 |
|------|------|---------|
| **routes/** | HTTP请求处理、参数校验、返回响应 | ❌ 不写业务逻辑,不直接调用第三方API |
| **services/** | 业务逻辑、第三方API调用、状态管理 | ❌ 不直接返回HTTP响应对象 |
| **models/** | 数据模型定义(请求/响应/领域模型) | ❌ 不包含业务逻辑 |
| **utils/** | 纯函数工具,可复用组件 | ❌ 不依赖特定业务逻辑 |
| **exceptions/** | 自定义异常类 | ❌ 不包含错误处理逻辑 |

### 2. 示例: 音频处理流程

```python
# routes/audio.py (路由层)
from fastapi import APIRouter, Depends
from app.models.requests import AudioRequest
from app.services import qwen_omni, mqtt_client

router = APIRouter()

@router.post("/process_audio")
async def process_audio(data: AudioRequest):
    # 仅负责参数校验和响应
    result = await qwen_omni.process_voice(data.audio_base64)
    await mqtt_client.publish_audio(result)
    return {"status": "ok"}

# services/qwen_omni.py (业务层)
async def process_voice(audio_base64: str) -> dict:
    # 1. 音频前置过滤
    if audio_filter.is_silence(audio_base64):
        return None  # 静音不处理

    # 2. 调用Qwen-Omni API
    response = await call_dashscope_api(...)

    # 3. 合成TTS并转MP3
    mp3_data = await tts.synthesize(response.text)

    return {"text": response.text, "audio": mp3_data}
```

---

## Naming Conventions

| 类型 | 命名规则 | 示例 |
|------|---------|------|
| 模块文件 | 小写+下划线 | `qwen_vl.py`, `mqtt_client.py` |
| 类名 | 大驼峰(PascalCase) | `class QwenVLService`, `class AudioRequest` |
| 函数名 | 小写+下划线 | `def process_audio()`, `async def call_api()` |
| 常量 | 大写+下划线 | `MAX_AUDIO_SIZE = 1024`, `DEFAULT_TTS_VOICE = "Cherry"` |
| 私有变量/函数 | 前缀单下划线 | `_internal_helper()`, `_cache = {}` |

---

## 配置管理

### 使用 `.env` + Pydantic Settings

```python
# app/config.py
from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    # DashScope配置
    dashscope_api_key: str
    dashscope_base_url: str = "https://dashscope.aliyuncs.com/compatible-mode/v1"

    # MQTT配置
    mqtt_broker: str = "mqtt://localhost:1883"
    mqtt_topic_audio: str = "emqx/esp32/{device_id}/audio"
    mqtt_topic_playaudio: str = "emqx/esp32/{device_id}/playaudio"

    # TTS配置
    tts_voice: str = "Cherry"
    mp3_bitrate: int = 32  # kbps

    class Config:
        env_file = ".env"

settings = Settings()
```

### .env 示例

```bash
# .env.example
DASHSCOPE_API_KEY=sk-xxxxxxxxxxxxxx
MQTT_BROKER=mqtt://emqx.cloud:1883
```

**规则**: 所有敏感信息和环境相关配置必须从`.env`读取,不得硬编码

---

## Examples

### 参考官方项目

基于 **PRD/Epic/Story** 要求,优先复用以下官方代码:

| 功能模块 | 参考代码 |
|---------|---------|
| 音频处理(INMP441+MQTT) | `emqx/esp32-mcp-mqtt-tutorial/samples/blog_4` |
| 图像上传+Qwen-VL调用 | `emqx/esp32-mcp-mqtt-tutorial/samples/blog_6` |
| FastAPI异步最佳实践 | FastAPI官方文档 - Async SQL Databases |

**禁止造轮子**: 开发前必须先查看上述官方代码,理解其实现再编写

---

## 核心原则

### ✅ DO

- 异步优先: 所有I/O操作使用 `async/await`
- 类型注解: 所有函数参数和返回值必须有类型提示
- 单一职责: 每个函数只做一件事
- 依赖注入: 使用FastAPI的`Depends`管理共享资源

### ❌ DON'T

- ❌ 不要在 `routes/` 中编写业务逻辑
- ❌ 不要硬编码配置(API Key、URL等)
- ❌ 不要使用全局变量共享状态
- ❌ 不要忽略类型提示

---

**总结**: 清晰分层、配置外部化、异步优先、复用官方代码

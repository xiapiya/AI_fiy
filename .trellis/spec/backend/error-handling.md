# Error Handling (V4.0 云端架构版)

> Python/FastAPI 错误处理规范
>
> **版本演进**: V4.0新增NewAPI网关错误、MQTTS连接错误、HTTPS上传错误处理

---

## Overview

基于 **PRD V4.0 要求**,错误处理必须保证:
1. **快速失败 + 兜底机制**: 大模型API超时/失败时立即下发预置音频,防止硬件无限等待
2. **详细日志**: 记录完整错误上下文,便于排查
3. **用户友好**: 错误响应格式统一,易于客户端处理
4. **云端容错**: NewAPI不可用时自动降级,MQTTS断线自动重连

**核心原则**: **永远不要让ESP32等待超过10秒**

---

## Error Types

### 1. 自定义异常类

```python
# app/exceptions/errors.py

class AudioProcessingError(Exception):
    """音频处理相关错误(格式不支持、静音检测等)"""
    pass

class LLMAPIError(Exception):
    """大模型API调用错误(超时、限流、返回格式错误)"""
    def __init__(self, message: str, status_code: int = None, should_fallback: bool = True):
        self.message = message
        self.status_code = status_code
        self.should_fallback = should_fallback  # 是否触发兜底音频
        super().__init__(self.message)

class MQTTPublishError(Exception):
    """MQTT发布失败"""
    pass

class TTSError(Exception):
    """TTS合成或MP3转码失败"""
    pass

class NewAPIError(Exception):
    """NewAPI网关相关错误 (V4.0新增)"""
    def __init__(self, message: str, retry: bool = True):
        self.message = message
        self.retry = retry  # 是否可重试
        super().__init__(self.message)

class HTTPSUploadError(Exception):
    """HTTPS图片上传失败 (V4.0新增)"""
    pass
```

### 2. HTTP状态码映射

| 错误类型 | HTTP状态码 | 示例场景 |
|---------|-----------|---------|
| 参数校验失败 | 400 | 音频格式不支持 |
| 资源未找到 | 404 | 设备ID不存在 |
| API限流 | 429 | DashScope调用频率超限 |
| 外部API超时 | 504 | Qwen-VL调用超过10s |
| 内部错误 | 500 | 未捕获的异常 |
| 服务不可用 | 503 | MQTT Broker宕机 |

---

## Error Handling Patterns

### 1. 全局异常处理器

```python
# app/main.py
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from app.exceptions.errors import LLMAPIError, AudioProcessingError
from app.services.fallback import send_fallback_audio
from app.utils.logger import log_error

app = FastAPI()

@app.exception_handler(LLMAPIError)
async def llm_api_exception_handler(request: Request, exc: LLMAPIError):
    """大模型API异常处理"""
    # 记录错误
    log_error("llm_api_error", exc, context={"path": request.url.path})

    # 触发兜底音频
    if exc.should_fallback:
        await send_fallback_audio(error_type="api_error")

    return JSONResponse(
        status_code=exc.status_code or 500,
        content={
            "error": "llm_api_error",
            "message": exc.message,
            "fallback_sent": exc.should_fallback
        }
    )

@app.exception_handler(AudioProcessingError)
async def audio_exception_handler(request: Request, exc: AudioProcessingError):
    """音频处理异常(通常不需要兜底)"""
    log_error("audio_processing_error", exc)
    return JSONResponse(
        status_code=400,
        content={"error": "audio_processing_error", "message": str(exc)}
    )

@app.exception_handler(Exception)
async def generic_exception_handler(request: Request, exc: Exception):
    """兜底异常处理器"""
    log_error("unexpected_error", exc, context={"path": request.url.path})
    await send_fallback_audio(error_type="internal_error")
    return JSONResponse(
        status_code=500,
        content={"error": "internal_error", "message": "服务器内部错误"}
    )
```

### 2. 服务层错误处理(带超时)

```python
# app/services/qwen_vl.py
import asyncio
from openai import AsyncOpenAI
from app.exceptions.errors import LLMAPIError
from app.config import settings

async def call_qwen_vl(image_url: str, prompt: str) -> dict:
    """
    调用Qwen-VL多模态大模型

    异常处理策略:
    1. 超时(10s) → 触发兜底音频
    2. API错误 → 触发兜底音频
    3. 返回格式错误 → 使用默认回复
    """
    client = AsyncOpenAI(
        api_key=settings.dashscope_api_key,
        base_url=settings.dashscope_base_url
    )

    try:
        # PRD要求: 大模型调用必须设置10秒超时
        response = await asyncio.wait_for(
            client.chat.completions.create(
                model="qwen-vl-max",
                messages=[
                    {
                        "role": "system",
                        "content": "你是情感陪伴助手,回复必须控制在20字以内,并包含情绪标签"
                    },
                    {
                        "role": "user",
                        "content": [
                            {"type": "image_url", "image_url": {"url": image_url}},
                            {"type": "text", "text": prompt}
                        ]
                    }
                ],
                max_tokens=50
            ),
            timeout=10.0  # 10秒超时
        )

        # 解析响应
        text = response.choices[0].message.content
        # 简单情绪提取(实际应让大模型直接返回JSON)
        emotion = _extract_emotion(text)
        return {"text": text, "emotion": emotion}

    except asyncio.TimeoutError:
        # 超时立即抛出,由全局处理器触发兜底
        raise LLMAPIError("Qwen-VL API超时(10s)", status_code=504, should_fallback=True)

    except Exception as e:
        # 其他API错误也触发兜底
        raise LLMAPIError(f"调用失败: {str(e)}", status_code=500, should_fallback=True)

def _extract_emotion(text: str) -> str:
    """从文本中提取情绪标签(简单规则)"""
    emotion_keywords = {
        "happy": ["开心", "高兴", "哈哈"],
        "sad": ["难过", "伤心"],
        "comfort": ["理解", "陪伴"]
    }
    for emotion, keywords in emotion_keywords.items():
        if any(kw in text for kw in keywords):
            return emotion
    return "neutral"
```

---

## Fallback Mechanism (兜底机制)

### PRD核心要求

> **任何大模型API失败都必须立即下发兜底音频,防止硬件无限期等待**

### 兜底音频库

```python
# app/services/fallback.py
from app.services.mqtt_client import MQTTClient
from app.config import settings
import base64

# 预先生成的MP3音频(Base64编码,避免TTS二次调用)
FALLBACK_AUDIOS = {
    "api_error": "UklGRiQAAABXQVZFZm10...",  # "抱歉,我刚刚走神了"
    "timeout": "UklGRiQAAABXQVZFZm10...",    # "让我想想..."
    "internal_error": "UklGRiQAAABXQVZFZm10...",  # "我有点累了,稍后再聊"
    "noise_detected": "UklGRiQAAABXQVZFZm10..."  # "我没听清,能再说一遍吗?"
}

async def send_fallback_audio(error_type: str, device_id: str = "default"):
    """
    发送兜底音频到ESP32

    Args:
        error_type: 错误类型(api_error/timeout/internal_error等)
        device_id: 设备ID
    """
    mqtt_client = MQTTClient()
    audio_base64 = FALLBACK_AUDIOS.get(error_type, FALLBACK_AUDIOS["internal_error"])

    await mqtt_client.publish(
        topic=f"emqx/esp32/{device_id}/playaudio",
        payload={"audio": audio_base64}
    )
```

### 使用示例

```python
# app/services/qwen_omni.py
from app.services.fallback import send_fallback_audio

async def process_voice(audio_data: str, device_id: str):
    try:
        # 调用大模型
        response = await call_qwen_omni_api(audio_data)
        return response
    except asyncio.TimeoutError:
        # 立即发送兜底音频
        await send_fallback_audio("timeout", device_id)
        raise LLMAPIError("API超时", should_fallback=False)  # 已手动处理,不再触发全局兜底
```

---

## API Error Responses

### 标准错误响应格式

```json
{
  "error": "llm_api_error",
  "message": "Qwen-VL API超时(10s)",
  "fallback_sent": true,
  "timestamp": "2026-04-05T14:30:00Z"
}
```

### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `error` | string | 错误类型(llm_api_error/audio_processing_error等) |
| `message` | string | 详细错误描述 |
| `fallback_sent` | boolean | 是否已下发兜底音频 |
| `timestamp` | string | 错误发生时间(ISO 8601) |

---

## Common Mistakes

### ❌ 错误做法

```python
# ❌ 不设置超时,可能永久阻塞
response = await client.chat.completions.create(...)

# ❌ 吞掉异常,不记录日志
try:
    await call_api()
except:
    pass  # 静默失败,无法排查

# ❌ API失败后不触发兜底,硬件一直等待
try:
    response = await call_api()
except Exception:
    return {"error": "failed"}  # ESP32会一直闪烁LED等待
```

### ✅ 正确做法

```python
# ✅ 设置超时
response = await asyncio.wait_for(client.chat.completions.create(...), timeout=10.0)

# ✅ 记录详细日志
except Exception as e:
    log_error("api_call_failed", e, context={"model": "qwen-vl-max"})
    raise

# ✅ 失败时立即发送兜底音频
except Exception as e:
    await send_fallback_audio("api_error", device_id)
    raise LLMAPIError(str(e), should_fallback=False)
```

---

## 核心原则

### ✅ DO

- 所有外部API调用必须设置超时(推荐10s)
- 失败时立即触发兜底音频,不让硬件等待
- 使用自定义异常类,携带业务上下文
- 记录详细错误日志(见 logging-guidelines.md)

### ❌ DON'T

- ❌ 不要吞掉异常,必须处理或向上抛出
- ❌ 不要忘记设置超时
- ❌ 不要在异常中暴露敏感信息(API Key等)
- ❌ 不要让ESP32等待超过10秒

---

**总结**: 快速失败 + 兜底保护 + 详细日志 = 可靠的错误处理

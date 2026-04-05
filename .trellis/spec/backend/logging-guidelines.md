# Logging Guidelines

> Python/FastAPI 日志规范

---

## Overview

使用 **Python标准库 logging** + **JSON格式化**,实现结构化日志

**核心原则**: 可追踪、可搜索、不泄密

---

## Log Levels

| 级别 | 使用场景 | 示例 |
|------|---------|------|
| **DEBUG** | 详细的调试信息(生产环境关闭) | 函数参数、中间变量 |
| **INFO** | 正常流程关键节点 | 请求开始/结束、大模型调用成功 |
| **WARNING** | 潜在问题(不影响功能) | API响应慢、缓存未命中 |
| **ERROR** | 错误但已处理 | 大模型API超时(已触发兜底) |
| **CRITICAL** | 严重错误(服务不可用) | MQTT Broker宕机 |

---

## Structured Logging

### 配置

```python
# app/utils/logger.py
import logging
import json
from datetime import datetime

class JSONFormatter(logging.Formatter):
    """JSON格式化器"""
    def format(self, record):
        log_data = {
            "timestamp": datetime.utcnow().isoformat() + "Z",
            "level": record.levelname,
            "message": record.getMessage(),
            "module": record.module,
            "function": record.funcName
        }
        # 添加额外上下文
        if hasattr(record, "context"):
            log_data["context"] = record.context
        return json.dumps(log_data, ensure_ascii=False)

def setup_logger():
    """初始化日志配置"""
    logger = logging.getLogger("backend")
    logger.setLevel(logging.INFO)

    handler = logging.StreamHandler()
    handler.setFormatter(JSONFormatter())
    logger.addHandler(handler)

    return logger

logger = setup_logger()
```

### 使用示例

```python
# app/services/qwen_vl.py
from app.utils.logger import logger

async def call_qwen_vl(image_url: str):
    logger.info("开始调用Qwen-VL", extra={"context": {"image_url": image_url[:50]}})

    try:
        response = await api_call()
        logger.info("Qwen-VL调用成功", extra={"context": {"latency_ms": 1234}})
        return response
    except Exception as e:
        logger.error("Qwen-VL调用失败", extra={"context": {"error": str(e)}})
        raise
```

### 输出格式

```json
{
  "timestamp": "2026-04-05T14:30:00Z",
  "level": "INFO",
  "message": "开始调用Qwen-VL",
  "module": "qwen_vl",
  "function": "call_qwen_vl",
  "context": {"image_url": "https://..."}
}
```

---

## What to Log

### ✅ 必须记录

| 场景 | 日志级别 | 内容 |
|------|---------|------|
| HTTP请求 | INFO | 路径、方法、响应时间 |
| 大模型调用 | INFO | 模型名、耗时、token数 |
| MQTT消息 | INFO | 主题、payload大小 |
| 错误/异常 | ERROR | 错误类型、堆栈、上下文 |
| 兜底音频触发 | WARNING | 触发原因、设备ID |

### 示例

```python
# app/routes/audio.py
from app.utils.logger import logger
import time

@router.post("/process_audio")
async def process_audio(request: Request):
    start_time = time.time()
    device_id = request.headers.get("X-Device-ID", "unknown")

    logger.info("接收音频处理请求", extra={
        "context": {"device_id": device_id, "path": request.url.path}
    })

    try:
        result = await service.process(...)
        latency = (time.time() - start_time) * 1000
        logger.info("音频处理完成", extra={
            "context": {"device_id": device_id, "latency_ms": latency}
        })
        return result
    except Exception as e:
        logger.error("音频处理失败", extra={
            "context": {"device_id": device_id, "error": str(e)}
        })
        raise
```

---

## What NOT to Log

### ❌ 禁止记录

- **敏感信息**: API Key、密码、Token
- **用户隐私**: 完整音频内容、图像URL(可记录前50字符)
- **大数据**: Base64音频/图像(仅记录大小)

### 示例

```python
# ❌ 错误: 记录API Key
logger.info(f"使用API Key: {settings.dashscope_api_key}")

# ✅ 正确: 仅记录是否配置
logger.info(f"API Key已配置: {bool(settings.dashscope_api_key)}")

# ❌ 错误: 记录完整音频数据
logger.info(f"音频数据: {audio_base64}")

# ✅ 正确: 仅记录大小
logger.info(f"音频大小: {len(audio_base64)} bytes")
```

---

## 核心原则

### ✅ DO

- 使用结构化日志(JSON格式)
- 记录关键业务指标(耗时、成功率)
- 包含上下文信息(device_id, request_id)

### ❌ DON'T

- ❌ 不要记录敏感信息
- ❌ 不要使用print()调试
- ❌ 不要记录过多Debug日志到生产环境

---

**总结**: 结构化 + 可搜索 + 保护隐私

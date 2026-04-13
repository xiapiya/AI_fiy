# Backend Development Guidelines (V4.0 云端架构演进版)

> Python/FastAPI云端开发规范
>
> **版本演进**: V4.0引入1Panel容器化管理、NewAPI模型网关、公网加密通信

---

## Overview

**技术栈**: Python + FastAPI + NewAPI + 1Panel + Docker
**部署架构**: Linux云服务器 + 容器化管理 + 公网加密传输
**核心职责**:
- 音频/图像数据处理与路由
- 通过NewAPI网关调度大模型 (Qwen-VL + Qwen-Omni)
- MQTT消息路由与下发
- TTS音频生成与转码

**核心原则**:
1. **安全第一** - 所有大模型请求通过NewAPI内网网关，不直连公网
2. **异步优先** - 所有I/O操作使用async/await
3. **容器化** - 使用1Panel统一管理EMQX、NewAPI、MySQL等服务
4. **快速失败** - 大模型调用必须10秒超时，失败立即触发兜底音频

---

## 云端架构 (V4.0)

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

**关键设计**:
- **EMQX**: 处理MQTTS长连接，TLS加密
- **FastAPI**: 处理HTTPS请求，调用NewAPI
- **NewAPI**: 统一管理大模型API Key，内网转发
- **Nginx**: SSL证书管理，反向代理

---

## Guidelines Index

| Guide | Description | Status |
|-------|-------------|--------|
| [Directory Structure](./directory-structure.md) | FastAPI项目结构、分层设计 | ✅ 已填写 |
| [Cloud Deployment](./cloud-deployment.md) | 1Panel部署、容器化管理、SSL配置 | ✅ V4.0新增 |
| [Database Guidelines](./database-guidelines.md) | ORM patterns, queries, migrations | 待填写 |
| [Error Handling](./error-handling.md) | 错误类型、兜底机制、超时处理 | ✅ 已填写 |
| [Quality Guidelines](./quality-guidelines.md) | Code standards, forbidden patterns | 待填写 |
| [Logging Guidelines](./logging-guidelines.md) | 结构化日志、性能追踪 | ✅ 已填写 |

---

## NewAPI 模型网关规范 (V4.0 核心)

### 为什么需要NewAPI?

**问题**: 直接在ESP32或FastAPI中硬编码DashScope API Key存在风险
- 泄露风险高（代码提交、日志记录）
- 多项目管理困难
- 无法统一控制并发和账单

**解决方案**: 部署NewAPI作为内网模型网关
- FastAPI只与NewAPI内网端口通信 (`http://localhost:3000`)
- NewAPI统一管理所有大模型API Key
- 支持负载均衡、并发控制、账单收敛

### 配置示例

**1Panel部署NewAPI容器**:
```yaml
# docker-compose.yml
version: '3.8'
services:
  newapi:
    image: calciumion/new-api:latest
    container_name: newapi
    ports:
      - "3000:3000"  # 仅内网访问
    environment:
      - SESSION_SECRET=your_secret
      - SQL_DSN=root:password@tcp(mysql:3306)/newapi
    restart: always
```

**FastAPI调用NewAPI**:
```python
# app/services/newapi_client.py
from openai import AsyncOpenAI

client = AsyncOpenAI(
    api_key="sk-xxx",  # NewAPI生成的内部Key
    base_url="http://localhost:3000/v1"  # 内网端口
)

async def call_qwen_vl(image_url: str, prompt: str):
    response = await client.chat.completions.create(
        model="qwen-vl-max",  # NewAPI中配置的模型名
        messages=[
            {"role": "user", "content": [
                {"type": "image_url", "image_url": {"url": image_url}},
                {"type": "text", "text": prompt}
            ]}
        ],
        max_tokens=50
    )
    return response.choices[0].message.content
```

---

## 核心开发原则

### ✅ DO

- **所有大模型请求必须通过NewAPI**, 不得直连DashScope
- **所有API调用必须设置10秒超时**, 失败立即触发兜底
- **使用1Panel管理容器**, 不要手动docker命令
- **SSL证书统一在Nginx配置**, FastAPI内部使用HTTP
- **使用结构化JSON日志**, 便于搜索和监控

### ❌ DON'T

- ❌ 不要在代码中硬编码API Key
- ❌ 不要让大模型调用阻塞超过10秒
- ❌ 不要忘记配置MQTT断线重连
- ❌ 不要在日志中记录敏感信息（API Key、完整音频）

---

## V4.0 核心变化

| 方面 | V3.2 (局域网) | V4.0 (公网云端) |
|-----|--------------|----------------|
| **通信协议** | MQTT (明文) | MQTTS + HTTPS (加密) |
| **部署方式** | 本地Docker | 1Panel容器化管理 |
| **模型调用** | 直连DashScope | 通过NewAPI网关 |
| **图片传输** | MQTT Base64 | HTTPS POST直传 |
| **多端支持** | 仅ESP32 | ESP32 + Android + Web |
| **安全性** | 局域网无加密 | TLS/SSL端到端加密 |

---

**总结**: 云端架构 + 容器化管理 + 安全加密 + NewAPI网关 = 生产级系统

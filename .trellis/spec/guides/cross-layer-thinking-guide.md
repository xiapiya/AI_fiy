# Cross-Layer Thinking Guide (V4.0 云端架构版)

> **Purpose**: Think through data flow across layers before implementing.
>
> **版本演进**: V4.0新增公网通信安全检查、云端链路检查、多端协同检查

---

## The Problem

**Most bugs happen at layer boundaries**, not within layers.

Common cross-layer bugs:
- API returns format A, frontend expects format B
- Database stores X, service transforms to Y, but loses data
- Multiple layers implement the same logic differently
- **V4.0新增**: ESP32端TLS证书与云端不匹配,导致握手失败
- **V4.0新增**: NewAPI不可用,但FastAPI未做降级处理
- **V4.0新增**: MQTT下发JSON格式与ESP32端解析不一致

---

## Before Implementing Cross-Layer Features

### Step 1: Map the Data Flow

Draw out how data moves:

```
Source → Transform → Store → Retrieve → Transform → Display
```

For each arrow, ask:
- What format is the data in?
- What could go wrong?
- Who is responsible for validation?

### Step 2: Identify Boundaries

| Boundary | Common Issues |
|----------|---------------|
| API ↔ Service | Type mismatches, missing fields |
| Service ↔ Database | Format conversions, null handling |
| Backend ↔ Frontend | Serialization, date formats |
| Component ↔ Component | Props shape changes |

### Step 3: Define Contracts

For each boundary:
- What is the exact input format?
- What is the exact output format?
- What errors can occur?

---

## Common Cross-Layer Mistakes

### Mistake 1: Implicit Format Assumptions

**Bad**: Assuming date format without checking

**Good**: Explicit format conversion at boundaries

### Mistake 2: Scattered Validation

**Bad**: Validating the same thing in multiple layers

**Good**: Validate once at the entry point

### Mistake 3: Leaky Abstractions

**Bad**: Component knows about database schema

**Good**: Each layer only knows its neighbors

---

## Checklist for Cross-Layer Features

Before implementation:
- [ ] Mapped the complete data flow
- [ ] Identified all layer boundaries
- [ ] Defined format at each boundary
- [ ] Decided where validation happens

After implementation:
- [ ] Tested with edge cases (null, empty, invalid)
- [ ] Verified error handling at each boundary
- [ ] Checked data survives round-trip

---

## V4.0 公网架构跨层检查清单

### 公网通信安全检查

在实现任何公网通信功能前,必须检查:

**ESP32端**:
- [ ] MQTTS是否正确配置TLS证书? (使用`esp_crt_bundle`或烧录CA证书)
- [ ] MbedTLS内存是否分配到PSRAM? (防止TLS握手OOM)
- [ ] HTTPS上传是否携带鉴权Token? (防止未授权上传)
- [ ] 敏感数据(图片)是否走HTTPS而非MQTT? (防止MQTT传输大文件阻塞)

**云端FastAPI**:
- [ ] 是否通过NewAPI而非直连大模型? (密钥安全)
- [ ] NewAPI不可用时是否有降级策略? (触发兜底音频)
- [ ] MQTTS客户端是否启用TLS? (与EMQX端口8883匹配)
- [ ] SSL证书是否在Nginx统一管理? (FastAPI内部使用HTTP)

### 云端链路检查

**数据流完整性**:
- [ ] MQTT下发的JSON格式是否与ESP32端解析一致?
  - 字段名匹配 (text/emotion/status)
  - 数据类型匹配 (字符串/整数/布尔)
- [ ] 延迟补偿状态节点是否完整?
  - "TLS握手中" → "云端同步中" → "推理中" → "回复中"
  - ESP32端状态机是否有对应状态?
- [ ] 错误处理是否覆盖网络抖动场景?
  - MQTTS断线自动重连
  - HTTPS上传超时重试
  - NewAPI不可用触发兜底

**性能追踪**:
- [ ] 是否记录全链路耗时日志?
  - ESP32上传音频 → FastAPI接收 → NewAPI调用 → TTS生成 → MQTT下发
  - 每个阶段耗时是否记录到日志和MQTT广播?

### 多端协同检查

**Android/Web端**:
- [ ] 主题订阅是否正确?
  - Android订阅: `emqx/esp32/{deviceId}/ui` (接收设备状态)
  - Web订阅: `emqx/system/logs` (只读日志)
- [ ] 远程指令优先级是否高于VAD触发?
  - ESP32端是否实现优先级队列? (远程指令 > VAD)
  - FreeRTOS Timer是否实现3秒软冷却防冲撞?
- [ ] 多端并发控制是否防止冲突?
  - Android "静音硬件" 是否生效?
  - Web端是否只读不下发控制指令?

### V4.0 典型跨层Bug案例

| Bug描述 | 涉及层级 | 根因 | 解决方案 |
|--------|---------|-----|----------|
| ESP32 TLS握手失败重启 | ESP32 ↔ EMQX | MbedTLS内存未分配到PSRAM | menuconfig强制PSRAM分配 |
| NewAPI调用失败无兜底 | FastAPI ↔ NewAPI | 未捕获NewAPIError异常 | 添加全局异常处理器 |
| Android下发拍照指令无响应 | Android ↔ ESP32 | 主题名拼写错误 | 统一主题常量定义 |
| Web端看不到实时日志 | FastAPI ↔ Web | 未启用MQTT日志广播 | 添加MQTTLogHandler |

---

## When to Create Flow Documentation

Create detailed flow docs when:
- Feature spans 3+ layers
- Multiple teams are involved
- Data format is complex
- Feature has caused bugs before
- **V4.0新增**: 涉及公网加密通信或多端协同

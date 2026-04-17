# FastAPI Vision接口和NewAPI集成

## 目标
实现云端FastAPI后端的视觉识别接口，集成NewAPI网关调用Qwen-VL多模态大模型，处理ESP32上传的图片并返回情感化音频响应。

## 开发类型
**后端开发** (Backend - Python/FastAPI)

---

## 功能需求

### 核心功能
- [ ] 实现 `POST /api/v1/vision/upload` HTTPS接口
- [ ] 接收ESP32上传的JPEG图片（<300KB）
- [ ] 通过NewAPI网关调用Qwen-VL-Max进行视觉识别
- [ ] 生成简短情感化文本回复（≤20字）
- [ ] 调用TTS生成MP3音频（Cherry音色，32kbps）
- [ ] 通过MQTTS下发音频和UI更新到ESP32
- [ ] 全链路延迟<3秒

### 辅助功能
- [ ] 请求鉴权（JWT/Token）
- [ ] 错误处理和兜底机制
- [ ] 结构化日志记录
- [ ] MQTT状态广播（processing/completed）

---

## 技术需求

### 1. FastAPI项目结构
按照 `.trellis/spec/backend/directory-structure.md` 规范创建：

```
backend/
├── app/
│   ├── main.py              # FastAPI应用入口
│   ├── config.py            # 配置管理（NewAPI地址、MQTT等）
│   ├── api/v1/              # API路由层
│   │   ├── __init__.py
│   │   └── vision.py        # POST /api/v1/vision/upload
│   ├── services/            # 业务逻辑层
│   │   ├── __init__.py
│   │   ├── newapi_client.py # NewAPI网关客户端
│   │   ├── qwen_vl.py       # Qwen-VL视觉识别
│   │   ├── tts.py           # TTS音频合成
│   │   ├── mqtt_client.py   # MQTTS客户端
│   │   └── fallback.py      # 兜底音频管理
│   ├── models/              # Pydantic数据模型
│   │   ├── __init__.py
│   │   └── vision.py        # 请求/响应模型
│   ├── core/                # 核心配置
│   │   ├── __init__.py
│   │   ├── security.py      # JWT认证
│   │   └── logging.py       # 日志配置
│   ├── utils/               # 工具函数
│   │   └── image.py         # 图片处理工具
│   └── exceptions/          # 自定义异常
│       └── __init__.py
├── tests/                   # 测试代码
├── deploy/                  # 部署配置
├── certs/                   # SSL证书
│   └── ca.pem              # MQTTS CA证书
├── requirements.txt
├── .env.example
├── Dockerfile
└── README.md
```

### 2. NewAPI集成
- 使用OpenAI SDK连接NewAPI网关
- 内网地址: `http://localhost:3000/v1`
- 调用模型: `qwen-vl-max`（需在NewAPI中预先配置）
- 超时设置: 10秒强制超时
- 失败兜底: 返回预置音频

### 3. 视觉识别流程
```python
# 伪代码示例
async def process_vision(image_data: bytes) -> dict:
    # 1. 保存图片到临时文件或OSS
    image_url = await upload_to_storage(image_data)

    # 2. 通过NewAPI调用Qwen-VL
    prompt = "请用10字以内描述这张图片，并给出情绪标签(happy/sad/comfort/thinking/neutral)"
    result = await newapi_client.call_qwen_vl(
        image_url=image_url,
        prompt=prompt,
        timeout=10  # 10秒超时
    )

    # 3. 解析返回JSON
    # 预期格式: {"text": "你在看书呢", "emotion": "happy"}
    response = parse_ai_response(result)

    # 4. TTS合成音频
    audio_data = await tts_service.synthesize(
        text=response["text"],
        voice="cherry",
        format="mp3"
    )

    # 5. MQTT下发
    await mqtt_client.publish_ui(
        emotion=response["emotion"],
        text=response["text"]
    )
    await mqtt_client.publish_audio(audio_data)

    return response
```

### 4. TTS音频合成
- 使用阿里云/讯飞/CosyVoice等TTS服务
- 音色: Cherry或类似的温暖女声
- 格式: MP3, 32kbps
- Base64编码后MQTT下发

### 5. MQTTS通信
- 订阅主题: `emqx/esp32/{device_id}/vision`
- 下发UI主题: `emqx/esp32/{device_id}/ui`
- 下发音频主题: `emqx/esp32/{device_id}/audio`
- TLS加密连接

---

## 验收标准

### 功能验收
- [ ] 接收ESP32 HTTPS POST上传的JPEG图片
- [ ] 通过NewAPI成功调用Qwen-VL
- [ ] 返回包含emotion和text的JSON
- [ ] TTS音频生成并下发到ESP32
- [ ] 全链路延迟<3秒（95%情况）

### 性能验收
- [ ] Qwen-VL调用10秒超时
- [ ] 失败时立即触发兜底音频
- [ ] 并发处理≥5个请求/秒
- [ ] 无内存泄漏

### 安全验收
- [ ] HTTPS加密传输
- [ ] JWT/Token鉴权
- [ ] NewAPI API Key不泄漏
- [ ] MQTTS TLS加密连接

### 日志验收
- [ ] 结构化JSON日志
- [ ] 记录全链路耗时
- [ ] 记录错误和异常
- [ ] MQTT广播关键状态

---

## 技术依赖

### Python依赖
```
fastapi==0.109.0
uvicorn[standard]==0.27.0
pydantic==2.5.0
openai==1.12.0          # NewAPI调用
paho-mqtt==1.6.1        # MQTTS客户端
python-jose[cryptography]  # JWT认证
python-multipart        # 文件上传
aiofiles                # 异步文件操作
requests                # HTTP请求（备用）
pydub                   # 音频处理
```

### 外部服务依赖
- NewAPI网关 (localhost:3000)
- EMQX MQTTS (公网或云端)
- Qwen-VL-Max (通过NewAPI)
- TTS服务 (阿里云/讯飞/CosyVoice)
- OSS存储 (可选，用于图片临时存储)

---

## 部署要求

### 1Panel部署 (参考 `.trellis/spec/backend/cloud-deployment.md`)
- Docker容器化部署
- Nginx反向代理
- SSL证书配置
- 环境变量管理

### 环境变量配置
```bash
# .env示例
# NewAPI配置
NEWAPI_BASE_URL=http://newapi:3000/v1
NEWAPI_API_KEY=sk-xxxxxx

# MQTT配置
MQTT_BROKER=mqtts://yourdomain.com:8883
MQTT_USERNAME=admin
MQTT_PASSWORD=xxxxxx
MQTT_CA_CERT=/app/certs/ca.pem

# TTS配置
TTS_PROVIDER=aliyun  # aliyun/xunfei/cosyvoice
TTS_API_KEY=xxxxxx
TTS_VOICE=cherry

# 安全配置
JWT_SECRET_KEY=xxxxxx
JWT_ALGORITHM=HS256

# OSS配置（可选）
OSS_ENDPOINT=oss-cn-hangzhou.aliyuncs.com
OSS_ACCESS_KEY=xxxxxx
OSS_SECRET_KEY=xxxxxx
OSS_BUCKET=vision-images
```

---

## 技术风险

### 1. NewAPI网关单点故障
- **风险**: NewAPI服务宕机导致所有大模型调用失败
- **应对**: 实现兜底音频机制，失败时立即返回预置音频

### 2. Qwen-VL调用延迟
- **风险**: 大模型推理耗时可能>10秒
- **应对**: 强制10秒超时，超时立即触发兜底

### 3. MQTT连接不稳定
- **风险**: 公网MQTT可能断连
- **应对**: 实现自动重连机制，重连间隔指数退避

### 4. 图片存储问题
- **风险**: 本地存储可能磁盘满
- **应对**: 使用OSS对象存储，或定期清理临时文件

### 5. TTS服务额度耗尽
- **风险**: TTS API调用次数超限
- **应对**: 实现本地缓存，相同文本复用音频

---

## 实施步骤 (待Research和Implement阶段细化)

### Step 1: 项目框架搭建
- 创建FastAPI项目目录结构
- 配置依赖和环境变量
- 实现基础日志和配置管理

### Step 2: NewAPI集成
- 实现 `newapi_client.py`
- 测试Qwen-VL调用
- 实现超时和重试机制

### Step 3: Vision接口实现
- 实现 `/api/v1/vision/upload` 路由
- 实现图片接收和验证
- 实现视觉识别服务

### Step 4: TTS集成
- 选择TTS服务商
- 实现 `tts.py` 服务
- 实现音频格式转换

### Step 5: MQTT集成
- 实现MQTTS客户端
- 实现UI和音频下发
- 实现状态广播

### Step 6: 测试和优化
- 单元测试
- 集成测试
- 压力测试
- 性能优化

### Step 7: 部署上线
- Docker镜像构建
- 1Panel部署
- SSL证书配置
- 监控告警配置

---

## 参考规范文档

- `.trellis/spec/backend/index.md` - 后端开发总览
- `.trellis/spec/backend/directory-structure.md` - 项目结构规范
- `.trellis/spec/backend/cloud-deployment.md` - 云端部署指南
- `.trellis/spec/backend/error-handling.md` - 错误处理规范
- `.trellis/spec/backend/logging-guidelines.md` - 日志记录规范

---

## 后续任务依赖

**此任务阻塞**:
- ESP32摄像头开发 (Phase 3) - 需要vision/upload接口可用

**此任务依赖**:
- 无 - 可独立开发和测试
- 建议等TFT屏幕完成后再开始实施

---

## 预估工作量
- 项目框架: 1人天
- NewAPI集成: 1人天
- Vision接口: 2人天
- TTS集成: 1人天
- MQTT集成: 1人天
- 测试优化: 2人天
- 部署上线: 1人天
- **总计**: 8-10人天

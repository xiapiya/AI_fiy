# FastAPI Vision API - MVP版本

ESP32视觉识别接口的MVP实现，集成NewAPI网关调用Qwen-VL多模态大模型。

## 功能特性

- ✅ 接收ESP32上传的JPEG图片（<300KB）
- ✅ 调用NewAPI/Qwen-VL进行视觉识别
- ✅ 10秒超时+兜底机制
- ✅ 返回情感化文本和emotion标签
- ✅ 结构化JSON日志

## MVP范围说明

**当前实现**：
- POST /api/v1/vision/upload 接口
- NewAPI客户端集成
- 超时兜底机制
- 基础日志记录

**暂未实现**（后续补充）：
- TTS音频合成
- MQTT音频下发
- JWT鉴权

## 快速开始

### 1. 安装依赖

```bash
cd backend
pip install -r requirements.txt
```

### 2. 配置环境变量

```bash
cp .env.example .env
```

编辑 `.env` 文件，配置必需的环境变量：

```bash
# NewAPI配置
NEWAPI_BASE_URL=http://localhost:3000/v1
NEWAPI_API_KEY=sk-your-api-key-here

# 应用配置
DEBUG=True  # 开发环境设为True
```

### 3. 启动服务

```bash
# 开发模式（自动重载）
python -m app.main

# 或使用uvicorn
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

### 4. 访问接口文档

打开浏览器访问：
- Swagger UI: http://localhost:8000/docs
- ReDoc: http://localhost:8000/redoc

## API接口

### POST /api/v1/vision/upload

上传图片进行视觉识别。

**请求参数**：
- `image`: File (JPEG格式，<300KB)
- `device_id`: string (可选，ESP32设备ID)

**成功响应**：
```json
{
  "success": true,
  "emotion": "happy",
  "text": "你在看书呢",
  "processing_time": 2.3
}
```

**失败响应（兜底）**：
```json
{
  "success": false,
  "emotion": "neutral",
  "text": "网络有点慢，稍等一下",
  "error": "timeout",
  "processing_time": 10.0
}
```

### GET /api/v1/vision/health

健康检查接口。

**响应**：
```json
{
  "status": "ok",
  "service": "vision-api"
}
```

## 项目结构

```
backend/
├── app/
│   ├── main.py              # FastAPI应用入口
│   ├── config.py            # 配置管理
│   ├── api/v1/
│   │   ├── __init__.py
│   │   └── vision.py        # Vision接口
│   ├── services/
│   │   ├── __init__.py
│   │   ├── newapi_client.py # NewAPI客户端
│   │   └── fallback.py      # 兜底机制
│   ├── models/
│   │   ├── __init__.py
│   │   └── vision.py        # 数据模型
│   ├── core/
│   │   ├── __init__.py
│   │   └── logging.py       # 日志配置
│   └── exceptions/
│       ├── __init__.py
│       └── errors.py        # 自定义异常
├── tests/
├── requirements.txt
├── .env.example
└── README.md
```

## 兜底机制

当NewAPI调用失败或超时时，系统会自动触发兜底响应：

| 错误类型 | emotion | 兜底文本 |
|---------|---------|---------|
| timeout | neutral | 网络有点慢，稍等一下 |
| newapi_error | neutral | 让我想想 |
| invalid_image | neutral | 图片好像有点问题 |
| error | neutral | 我暂时看不太清楚 |

## 日志格式

系统使用结构化JSON日志：

```json
{
  "timestamp": "2025-04-16T10:30:00Z",
  "level": "INFO",
  "message": "NewAPI调用成功",
  "module": "newapi_client",
  "function": "call_qwen_vl",
  "context": {
    "device_id": "esp32-001",
    "result": "你在看书呢(happy)"
  }
}
```

## 测试接口

### 使用curl测试

```bash
# 上传图片
curl -X POST "http://localhost:8000/api/v1/vision/upload" \
  -H "Content-Type: multipart/form-data" \
  -F "image=@test_image.jpg" \
  -F "device_id=esp32-001"

# 健康检查
curl http://localhost:8000/api/v1/vision/health
```

### 使用Python测试

```python
import requests

url = "http://localhost:8000/api/v1/vision/upload"
files = {"image": open("test_image.jpg", "rb")}
data = {"device_id": "esp32-001"}

response = requests.post(url, files=files, data=data)
print(response.json())
```

## 代码质量

### 运行Lint

```bash
# Black代码格式化
black app/

# Ruff代码检查
ruff check app/
```

### 运行类型检查

```bash
# MyPy类型检查
mypy app/
```

## 环境变量说明

| 变量名 | 说明 | 默认值 |
|-------|------|--------|
| NEWAPI_BASE_URL | NewAPI网关地址 | http://localhost:3000/v1 |
| NEWAPI_API_KEY | NewAPI API密钥 | 必填 |
| NEWAPI_TIMEOUT | NewAPI调用超时（秒） | 10.0 |
| MAX_IMAGE_SIZE | 最大图片大小（字节） | 307200 (300KB) |
| DEBUG | 调试模式 | False |

## 常见问题

### 1. NewAPI连接失败

确保NewAPI服务正在运行：
```bash
# 检查NewAPI是否可访问
curl http://localhost:3000/v1/models
```

### 2. 图片上传失败

检查图片格式和大小：
- 仅支持JPEG格式
- 文件大小<300KB

### 3. 超时问题

调整超时配置：
```bash
# .env
NEWAPI_TIMEOUT=15.0  # 增加到15秒
```

## 下一步开发

- [ ] 集成TTS音频合成
- [ ] 实现MQTT音频下发
- [ ] 添加JWT鉴权
- [ ] 完善单元测试
- [ ] 添加Docker部署配置

## 技术栈

- FastAPI 0.109.0
- Python 3.11+
- OpenAI SDK 1.12.0
- Pydantic 2.5.0
- Uvicorn 0.27.0

## 相关文档

- [PRD文档](../.trellis/tasks/04-16-backend-vision-api/prd.md)
- [后端规范](../.trellis/spec/backend/)
- [日志规范](../.trellis/spec/backend/logging-guidelines.md)
- [错误处理](../.trellis/spec/backend/error-handling.md)

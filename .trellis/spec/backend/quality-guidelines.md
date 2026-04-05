# Quality Guidelines

> Python/FastAPI 代码质量标准

---

## Overview

基于 **PRD/Epic/Story** 要求,代码质量要求:
1. **禁止造轮子**: 优先复用官方代码
2. **代码审查**: 使用 codex MCP 进行审查
3. **类型安全**: 强制类型注解
4. **测试覆盖**: 核心功能必须有测试

---

## Forbidden Patterns

### ❌ 绝对禁止

| 反模式 | 原因 | 替代方案 |
|--------|------|---------|
| 硬编码配置 | 不灵活、泄密风险 | 使用 `.env` + `config.py` |
| 同步阻塞IO | 性能差 | 使用 `async/await` |
| 全局变量共享状态 | 线程不安全 | 使用依赖注入 |
| 忽略类型注解 | 难维护、易出错 | 所有函数必须有类型提示 |
| 造轮子 | 浪费时间、质量差 | **优先查看PRD文档轮子+联网搜索** |

### 示例

```python
# ❌ 错误: 硬编码API Key
api_key = "sk-1234567890"

# ✅ 正确: 从配置读取
from app.config import settings
api_key = settings.dashscope_api_key

# ❌ 错误: 同步IO
def process_audio():
    result = requests.post(...)  # 阻塞
    return result

# ✅ 正确: 异步IO
async def process_audio():
    result = await httpx_client.post(...)
    return result

# ❌ 错误: 无类型注解
def call_api(data):
    ...

# ✅ 正确: 类型注解
async def call_api(data: dict) -> dict:
    ...
```

---

## Required Patterns

### ✅ 必须遵循

| 模式 | 要求 | 示例 |
|------|------|------|
| 类型注解 | 所有函数参数+返回值 | `def foo(x: int) -> str` |
| 异步优先 | 所有I/O操作 | `async def`, `await` |
| 错误处理 | 自定义异常+超时 | `try/except` + `asyncio.wait_for` |
| 依赖注入 | 共享资源 | FastAPI `Depends()` |
| 配置外部化 | 敏感信息 | `.env` + `pydantic_settings` |

---

## Reuse-First Principle (禁止造轮子)

### 核心流程

```
开发新功能前:
1. 查看 PRD/Epic/Story 文档的"复用点"
2. 搜索 emqx/esp32-mcp-mqtt-tutorial (blog_4, blog_6)
3. 联网搜索现有库/框架
4. 确认无法复用后才自己实现
```

### 必须复用的官方代码

| 功能 | 官方代码路径 |
|------|------------|
| 音频处理+MQTT | `emqx/esp32-mcp-mqtt-tutorial/samples/blog_4` |
| 图像上传+Qwen-VL | `emqx/esp32-mcp-mqtt-tutorial/samples/blog_6` |
| DashScope OpenAI兼容调用 | 见blog_6代码 |

### 搜索关键词示例

- "python fastapi mqtt client async"
- "qwen-vl openai compatible python"
- "lameenc mp3 encode python"
- "dashscope tts python sdk"

---

## Testing Requirements

### 必须测试的模块

| 模块 | 测试类型 | 覆盖率要求 |
|------|---------|----------|
| services/qwen_vl.py | 单元测试 | ≥80% |
| services/audio_filter.py | 单元测试 | ≥90% |
| routes/* | 集成测试 | ≥70% |

### 测试示例

```python
# tests/test_qwen_vl.py
import pytest
from app.services.qwen_vl import call_qwen_vl
from app.exceptions.errors import LLMAPIError

@pytest.mark.asyncio
async def test_qwen_vl_success():
    """测试正常调用"""
    result = await call_qwen_vl("https://image.jpg", "这是什么?")
    assert "text" in result
    assert "emotion" in result

@pytest.mark.asyncio
async def test_qwen_vl_timeout():
    """测试超时处理"""
    with pytest.raises(LLMAPIError) as exc_info:
        await call_qwen_vl("https://slow.jpg", "测试", timeout=0.1)
    assert exc_info.value.status_code == 504
```

---

## Code Review Checklist (使用 codex MCP)

### 开发流程

```bash
# 1. 编写代码
vim app/services/new_feature.py

# 2. 运行代码审查 (使用 codex MCP)
# (在Claude Code中调用 mcp_codex 工具进行审查)

# 3. 修复审查发现的问题

# 4. 提交代码
git add app/services/new_feature.py
git commit -m "feat(service): add new feature"
```

### 审查检查项

- [ ] 所有函数有类型注解
- [ ] 所有I/O操作是异步的
- [ ] 外部API调用有超时设置
- [ ] 错误处理完整(try/except + 兜底)
- [ ] 敏感信息从配置读取
- [ ] 有必要的日志记录
- [ ] **已查看PRD文档的复用点**
- [ ] **已联网搜索现有轮子**

---

## Linting & Formatting

### 工具链

```bash
# requirements-dev.txt
black==24.3.0        # 代码格式化
mypy==1.9.0          # 类型检查
ruff==0.3.0          # 快速linter
pytest==8.1.0        # 测试框架
pytest-asyncio==0.23.0
```

### 运行检查

```bash
# 格式化代码
black app/

# 类型检查
mypy app/ --strict

# Linting
ruff check app/

# 运行测试
pytest tests/
```

### 配置示例

```toml
# pyproject.toml
[tool.black]
line-length = 100

[tool.mypy]
python_version = "3.11"
strict = true
warn_return_any = true

[tool.ruff]
line-length = 100
select = ["E", "F", "I"]  # pycodestyle, pyflakes, isort
```

---

## Common Mistakes

### ❌ 常见错误

1. **忘记设置超时**: `await client.create(...)` → 可能永久阻塞
2. **硬编码配置**: `api_key = "sk-xxx"` → 泄密风险
3. **不记录日志**: 异常发生无法排查
4. **造轮子**: 自己写MQTT客户端 → 质量差+浪费时间
5. **忽略类型检查**: 导致运行时错误

### ✅ 正确做法

1. 所有API调用设置10秒超时
2. 配置从 `.env` 读取
3. 关键节点记录INFO日志,错误记录ERROR日志
4. **开发前先查PRD文档+联网搜索轮子**
5. 运行 `mypy` 确保类型安全

---

## 核心原则

### ✅ DO

- 优先复用官方代码(blog_4, blog_6)
- 使用codex MCP审查代码
- 所有函数类型注解
- 外部API设置超时
- 配置外部化

### ❌ DON'T

- ❌ 不要硬编码配置
- ❌ 不要使用同步IO
- ❌ 不要忽略类型检查
- ❌ **不要造轮子,先搜索!**

---

**总结**: 类型安全 + 异步优先 + 复用为王 + 代码审查

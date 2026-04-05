# Reuse-First Principle (禁止造轮子)

> 开发前必读:复用优先原则

---

## 核心原则

**在编写任何代码之前,必须按以下顺序查找现有轮子:**

```
1. 查看PRD/Epic/Story文档的"复用点"章节
2. 搜索官方仓库(emqx/esp32-mcp-mqtt-tutorial)
3. 联网搜索现有库/框架/工具
4. 确认无法复用后,才可以自己实现
```

**违反此原则 = 代码审查不通过**

---

## 为什么禁止造轮子?

| 造轮子的代价 | 复用轮子的收益 |
|------------|-------------|
| 浪费时间(数小时→数天) | 节省时间(数分钟→数小时) |
| 质量差(未经验证) | 质量高(已被验证) |
| Bug多(边界case未覆盖) | Bug少(社区已修复) |
| 难维护(自己负责升级) | 易维护(社区持续更新) |

**示例**: 自己写MQTT客户端需要3天,复用官方库只需10分钟

---

## 开发前查找流程

### Step 1: 查看PRD文档的复用点

**所有三个文档(PRD/Epic/Story)都明确列出了复用点:**

#### 后端(Python/FastAPI)

| 功能 | 必须复用的代码 |
|------|-------------|
| 音频处理+MQTT | `emqx/esp32-mcp-mqtt-tutorial/samples/blog_4` |
| 图像上传+Qwen-VL | `emqx/esp32-mcp-mqtt-tutorial/samples/blog_6` |
| DashScope调用 | OpenAI兼容端点 `/compatible-mode/v1` |

#### 嵌入式(ESP32-S3)

| 功能 | 必须复用的代码 |
|------|-------------|
| I2S音频驱动 | blog_4代码 |
| OV2640摄像头 | blog_6代码 |
| LVGL UI | LVGL官方ESP32-S3示例 |
| MQTT客户端 | PubSubClient或blog_4使用的库 |

#### Android (Kotlin)

| 功能 | 必须复用的库 |
|------|------------|
| MQTT客户端 | HiveMQ MQTT Client |
| 本地数据库 | Room |
| 后台保活 | Foreground Service |

### Step 2: 搜索官方仓库

**GitHub搜索关键词示例:**

```bash
# 后端
"python fastapi mqtt async"
"qwen-vl openai compatible"
"dashscope python sdk"

# 嵌入式
"esp32-s3 i2s lvgl"
"esp32 ov2640 mqtt"
"lvgl tft_espi esp32-s3"

# Android
"kotlin mqtt hivemq"
"android room database foreground service"
```

### Step 3: 联网搜索现有库

使用 Claude Code 的 WebSearch 工具或自行搜索:

```
[开发者名称]: "我需要实现XXX功能,请搜索现有库"
[AI]: 使用WebSearch工具搜索...
```

### Step 4: 确认无法复用

**只有满足以下条件,才可以自己实现:**

- [ ] 已查看PRD/Epic/Story的复用点章节
- [ ] 已搜索官方仓库,未找到匹配代码
- [ ] 已联网搜索,未找到成熟库
- [ ] 功能极其特殊,确实无现成方案

---

## 代码审查检查项

### 使用 codex MCP 审查时,必须检查:

```
审查员: "这段代码是否复用了现有轮子?"

- [ ] 已查看PRD文档的复用点
- [ ] 已搜索官方仓库
- [ ] 已联网搜索现有库
- [ ] 如自己实现,说明为何无法复用
```

**如未遵循复用原则 → 要求重写**

---

## 常见违规示例

### ❌ 错误: 造轮子

```python
# ❌ 自己写MQTT客户端(浪费3天)
class MyMQTTClient:
    def connect(self):
        # 自己实现协议...
        pass
```

### ✅ 正确: 复用轮子

```python
# ✅ 使用成熟库(10分钟配置完成)
from paho.mqtt import client as mqtt_client

client = mqtt_client.Client()
client.connect("broker.emqx.io")
```

---

### ❌ 错误: 忽略官方代码

```cpp
// ❌ 自己写I2S驱动(浪费2天+Bug多)
void initI2S() {
    // 寄存器操作...
}
```

### ✅ 正确: 复用官方代码

```cpp
// ✅ 复用blog_4代码(30分钟)
#include "blog_4/i2s_driver.h"  // 直接复用
void setup() {
    initI2SFromBlog4();
}
```

---

## 核心原则总结

| 原则 | 说明 |
|------|------|
| **查PRD优先** | 文档已列出所有复用点 |
| **搜索其次** | GitHub + Google搜索 |
| **实现最后** | 确认无轮子才动手 |
| **必须审查** | codex MCP审查复用情况 |

---

**记住**: 30分钟搜索 > 3天造轮子

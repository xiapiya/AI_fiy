# Thinking Guides

> **Purpose**: Expand your thinking to catch things you might not have considered.

---

## AI交互规范 (AI Interaction Guidelines)

**重要**: 所有AI回复必须使用中文

- ✅ 使用中文回复所有问题和说明
- ✅ 代码注释可以使用英文
- ✅ 技术术语保持英文（如MQTT, I2S, FastAPI等）
- ❌ 不要使用英文进行对话和解释

---

## Why Thinking Guides?

**Most bugs and tech debt come from "didn't think of that"**, not from lack of skill:

- Didn't think about what happens at layer boundaries → cross-layer bugs
- Didn't think about code patterns repeating → duplicated code everywhere
- Didn't think about edge cases → runtime errors
- Didn't think about future maintainers → unreadable code
- **Didn't think about reusing existing code** →造轮子浪费时间

These guides help you **ask the right questions before coding**.

---

## Available Guides

| Guide | Purpose | When to Use |
|-------|---------|-------------|
| **[Reuse-First Principle](./reuse-first-principle.md)** | **禁止造轮子,优先复用** | **开发任何功能前必读** |
| **[Embedded Guidelines](./embedded-guidelines.md)** | **ESP32-S3嵌入式开发规范** | **开发硬件功能时必读** |
| **[LVGL ESP32 Guidelines](./lvgl-esp32-guidelines.md)** | **LVGL v9 + ESP32-S3开发规范** | **开发TFT UI功能时必读** |
| [Code Reuse Thinking Guide](./code-reuse-thinking-guide.md) | Identify patterns and reduce duplication | When you notice repeated patterns |
| [Cross-Layer Thinking Guide](./cross-layer-thinking-guide.md) | Think through data flow across layers | Features spanning multiple layers |

---

## Quick Reference: Thinking Triggers

### When to Think About Reuse (CRITICAL)

- [ ] **任何开发任务开始前** ← 最优先检查!
- [ ] 即将编写新功能/组件
- [ ] 需要集成第三方服务
- [ ] 看到相似代码或逻辑

→ **必读** [Reuse-First Principle](./reuse-first-principle.md)

### When to Think About Embedded Development

- [ ] 开发ESP32-S3硬件功能
- [ ] 集成I2S音频/摄像头/TFT屏幕
- [ ] 处理MQTT/HTTP通信
- [ ] 优化FreeRTOS任务调度

→ Read [Embedded Guidelines](./embedded-guidelines.md)

### When to Think About LVGL UI Development

- [ ] 开发TFT屏幕UI功能
- [ ] UI状态不更新/刷新延迟
- [ ] 容器背景色显示异常
- [ ] FreeRTOS状态监听任务
- [ ] LVGL线程安全问题

→ Read [LVGL ESP32 Guidelines](./lvgl-esp32-guidelines.md)

### When to Think About Cross-Layer Issues

- [ ] Feature touches 3+ layers (API, Service, Component, Database)
- [ ] Data format changes between layers
- [ ] Multiple consumers need the same data
- [ ] You're not sure where to put some logic

→ Read [Cross-Layer Thinking Guide](./cross-layer-thinking-guide.md)

### When to Think About Code Reuse

- [ ] You're writing similar code to something that exists
- [ ] You see the same pattern repeated 3+ times
- [ ] You're adding a new field to multiple places
- [ ] **You're modifying any constant or config**
- [ ] **You're creating a new utility/helper function** ← Search first!

→ Read [Code Reuse Thinking Guide](./code-reuse-thinking-guide.md)

---

## Pre-Modification Rule (CRITICAL)

> **Before changing ANY value, ALWAYS search first!**

```bash
# Search for the value you're about to change
grep -r "value_to_change" .
```

This single habit prevents most "forgot to update X" bugs.

---

## Code Review Workflow

### 开发完成后,必须使用 codex MCP 审查

```
1. 编写代码
2. 自测(lint + typecheck + 手动测试)
3. 调用 codex MCP 工具进行代码审查
4. 修复审查发现的问题
5. 提交代码
```

**审查检查项**(codex MCP会自动检查):
- [ ] 是否复用了现有轮子(查PRD文档+联网搜索)
- [ ] 类型注解完整
- [ ] 错误处理完善
- [ ] 配置外部化
- [ ] 日志记录清晰

---

## How to Use This Directory

1. **Before coding**: **必读** [Reuse-First Principle](./reuse-first-principle.md)
2. **During coding**: If something feels repetitive or complex, check the guides
3. **After coding**: Use codex MCP to review
4. **After bugs**: Add new insights to the relevant guide (learn from mistakes)

---

## Contributing

Found a new "didn't think of that" moment? Add it to the relevant guide.

---

**Core Principle**: 30分钟搜索 > 3天造轮子 + codex MCP审查 = 高质量代码

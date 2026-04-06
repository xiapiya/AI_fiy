# Journal 1

---

## Session 1: 初始化项目并填写开发规范

**Date**: 2026-04-05
**Commit**: `70764f9`
**Developer**: xiapiya

### 完成的工作

#### 1. 项目规范填写

基于PRD、Epic、User Stories三个文档,完成了完整的开发规范填写:

**后端规范** (`.trellis/spec/backend/`)
- `directory-structure.md`: Python/FastAPI分层架构、配置管理
- `error-handling.md`: 兜底机制、10秒超时、自定义异常
- `logging-guidelines.md`: JSON结构化日志、隐私保护
- `quality-guidelines.md`: 禁止造轮子、类型注解、codex MCP审查
- `database-guidelines.md`: 说明后端无数据库(Android端用Room)

**嵌入式规范** (`.trellis/spec/guides/`)
- `embedded-guidelines.md`: ESP32-S3硬件开发、状态机、LVGL UI、MQTT主题

**Android规范** (`.trellis/spec/frontend/`)
- `index.md`: Kotlin + HiveMQ MQTT + Room数据库 + Foreground Service

**通用原则** (`.trellis/spec/guides/`)
- `reuse-first-principle.md`: 禁止造轮子、开发前查找轮子、代码审查流程
- `index.md`: 更新索引,增加新规范链接

#### 2. 必须复用的官方代码

明确列出所有模块必须复用的代码:
- 音频处理: `emqx/esp32-mcp-mqtt-tutorial/samples/blog_4`
- 图像处理: `emqx/esp32-mcp-mqtt-tutorial/samples/blog_6`
- DashScope调用: OpenAI兼容端点
- LVGL UI: LVGL官方ESP32-S3示例

#### 3. 代码审查流程

在所有规范中集成了codex MCP代码审查流程:
- 开发后必须调用codex MCP工具审查
- 审查检查项:复用轮子、类型注解、错误处理、配置外部化

#### 4. Git仓库初始化

- 创建`.gitignore`(Python/ESP32/Android/IDE排除)
- 初始化git仓库
- 创建初始提交(98个文件,18867行)
- 推送到GitHub: https://github.com/xiapiya/AI_fiy.git

### 关键原则

**三大铁律**:
1. 禁止造轮子: 开发前查PRD文档+联网搜索
2. 必须复用: blog_4(音频)+blog_6(摄像头)+各成熟库
3. 代码审查: 写完代码后调用codex MCP审查

### 技术栈

- **后端**: Python/FastAPI + Qwen-VL/Omni + MQTT + lameenc
- **嵌入式**: ESP32-S3 + I2S音频 + OV2640摄像头 + LVGL + TFT屏幕
- **Android**: Kotlin + Room + HiveMQ MQTT + Foreground Service

### 文件统计

| 类别 | 文件数 | 说明 |
|------|-------|------|
| 项目文档 | 3个 | PRD、Epic、User Stories |
| 开发规范 | 13个 | 后端、嵌入式、Android完整规范 |
| Trellis框架 | 80+个 | 工作流、脚本、命令、Agent配置 |
| **总计** | 98个文件 | 18,867行代码/文档 |

### 后续工作

项目已具备完整的开发规范,可以开始具体功能开发:
- Phase 1: 硬件拼装 + MQTT/HTTP基础 + Fork官方blog_4+blog_6
- Phase 2: TFT+LVGL UI + 单模态语音闭环
- Phase 3: OV2640 + Qwen-VL视觉 + Web监控
- Phase 4: Android App + 历史持久化 + 多设备防冲突

---

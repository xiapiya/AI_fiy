# xiapiya 情感陪伴智能体 - Android 客户端 (V4.2 极简解耦版)

> 移动端文本对话数字分身，通过云端统一上下文实现跨端记忆同步

---

## 项目概述

这是 xiapiya 情感陪伴智能体项目的 Android 移动端客户端，采用极简解耦设计：

- **纯文本对话** - 专注文本交互体验
- **完全解耦** - 不依赖 ESP32 硬件状态
- **跨端记忆** - 通过 Session ID 共享云端上下文（可访问 ESP32 语音对话历史）
- **流式渲染** - SSE 实时逐字显示 AI 回复
- **免鉴权设计** - 硬编码 Session ID，快速迭代

---

## 技术栈

### 核心技术
- **语言**: Kotlin 1.9.22
- **架构**: MVVM (ViewModel + Repository + Room)
- **UI**: Material Design 3 + XML Layouts
- **异步**: Kotlin Coroutines + Flow
- **网络**: Retrofit 2.9.0 + OkHttp 4.12.0 + SSE
- **数据库**: Room 2.6.1
- **依赖注入**: 手动注入（Application 单例）

### 主要依赖
```kotlin
// 网络
implementation("com.squareup.retrofit2:retrofit:2.9.0")
implementation("com.squareup.okhttp3:okhttp-sse:4.12.0")

// 数据库
implementation("androidx.room:room-ktx:2.6.1")

// 架构组件
implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.7.0")
implementation("androidx.lifecycle:lifecycle-livedata-ktx:2.7.0")

// UI
implementation("com.google.android.material:material:1.11.0")
implementation("androidx.recyclerview:recyclerview:1.3.2")
```

---

## 项目结构

```
app/src/main/java/com/xiapiya/companion/
├── data/                      # 数据层
│   ├── local/                 # Room 数据库
│   │   ├── ChatMessage.kt     # 消息实体
│   │   ├── ChatDao.kt         # 数据访问对象
│   │   └── AppDatabase.kt     # 数据库实例
│   ├── remote/                # 网络层
│   │   ├── models/            # 数据模型
│   │   │   ├── ChatRequest.kt
│   │   │   └── ChatResponse.kt
│   │   ├── ChatApiService.kt  # Retrofit 接口
│   │   ├── SseClient.kt       # SSE 流式客户端
│   │   └── RetrofitClient.kt  # Retrofit 单例
│   └── repository/
│       └── ChatRepository.kt  # 数据仓库
├── domain/                    # 业务逻辑层（暂未使用）
│   ├── model/
│   └── usecase/
├── presentation/              # UI 层
│   ├── chat/
│   │   ├── ChatActivity.kt    # 聊天界面
│   │   ├── ChatViewModel.kt   # ViewModel
│   │   └── ChatAdapter.kt     # RecyclerView 适配器
│   └── common/
│       └── ViewModelFactory.kt
├── util/
│   └── Constants.kt           # 全局常量
└── CompanionApplication.kt    # Application 类
```

---

## 核心功能

### 1. 文本对话
- 用户输入文本发送到云端
- AI 回复流式渲染（逐字显示）
- 聊天记录本地持久化（Room）

### 2. 跨端记忆同步
- 硬编码 `SESSION_ID = "xiapiya_master_01"`
- 云端通过 Session ID 索引完整对话历史
- 可访问 ESP32 的语音对话记录（统一上下文）

### 3. SSE 流式渲染
- 实时接收 AI 回复文本块
- 逐字追加显示
- 提升用户体验

### 4. 极简 UI
- Material Design 3
- 左侧 AI 消息气泡（灰色）
- 右侧用户消息气泡（蓝色）
- 流式文本独立显示区域

---

## 配置说明

### 修改后端 API 地址

编辑 `util/Constants.kt`:

```kotlin
object AppConstants {
    // 修改为实际后端地址
    const val API_BASE_URL = "https://yourdomain.com"  // 生产环境
    // 或
    const val API_BASE_URL = "http://192.168.1.100:8000"  // 开发环境（本地IP）
}
```

### 修改 Session ID

如果需要管理多个设备，修改 `Constants.kt`:

```kotlin
object AppConstants {
    // 必须与 ESP32 的 DEVICE_ID 保持一致
    const val CLIENT_SESSION_ID = "your_custom_session_id"
}
```

---

## 后端接口依赖

Android 客户端依赖以下后端接口：

### 1. 文本对话接口（必需）
```
POST /api/v1/chat/text
Content-Type: application/json

Request:
{
    "session_id": "xiapiya_master_01",
    "content": "用户输入的文本",
    "timestamp": 1713514200
}

Response:
{
    "reply": "AI 回复的文本",
    "emotion": "happy",
    "timestamp": 1713514205
}
```

### 2. SSE 流式接口（推荐）
```
POST /api/v1/chat/stream
Content-Type: application/json
Accept: text/event-stream

Response:
event: message
data: 你

event: message
data: 好

event: done
data: {"total_tokens": 42}
```

---

## 开发指南

### 在 Android Studio 中打开项目

1. 打开 Android Studio
2. File → Open → 选择 `android/xiapiya-companion` 目录
3. 等待 Gradle 同步完成

### 编译运行

```bash
# 编译 Debug 版本
./gradlew assembleDebug

# 安装到设备
./gradlew installDebug

# 或直接运行
./gradlew run
```

### 运行测试

```bash
# 单元测试
./gradlew test

# UI 测试
./gradlew connectedAndroidTest
```

---

## Mock 开发模式

如果后端接口未就绪，可以修改 `ChatRepository.kt` 使用 Mock 数据：

```kotlin
suspend fun sendMessageSync(
    sessionId: String,
    content: String
): Result<String> = withContext(Dispatchers.IO) {
    try {
        // 1. 保存用户消息
        chatDao.insert(ChatMessage(...))

        // 2. Mock AI 回复（开发阶段）
        delay(1000)  // 模拟网络延迟
        val mockReply = "这是 Mock 回复：你说的是「$content」"

        // 3. 保存 Mock 回复
        chatDao.insert(ChatMessage(
            content = mockReply,
            isFromUser = false,
            ...
        ))

        Result.success(mockReply)
    } catch (e: Exception) {
        Result.failure(e)
    }
}
```

---

## 当前状态

### ✅ 已完成（Phase 1）
- [x] 项目框架搭建
- [x] Gradle 配置
- [x] MVVM 包结构
- [x] Room 数据库（Entity + Dao + Database）
- [x] Retrofit + SSE 网络层
- [x] Repository 模式
- [x] ViewModel + LiveData
- [x] ChatActivity + UI 布局
- [x] RecyclerView Adapter
- [x] Material Design 样式

### 🔄 进行中（Phase 2-3）
- [ ] 后端接口联调
- [ ] SSE 流式渲染测试
- [ ] 跨端上下文验证
- [ ] 网络异常处理
- [ ] 单元测试编写

### 📋 待开发（Phase 4-7）
- [ ] 弱网优化
- [ ] 深色模式支持
- [ ] 多设备管理
- [ ] 代码优化
- [ ] 性能测试
- [ ] APK 打包发布

---

## 面试展示要点

### 技术亮点
- ✅ **MVVM 架构** - 规范的分层设计
- ✅ **Kotlin 协程** - 异步编程最佳实践
- ✅ **SSE 流式渲染** - 实时用户体验
- ✅ **Room 数据库** - 本地持久化
- ✅ **跨端协同** - 分布式系统设计
- ✅ **Clean Code** - 代码质量和可维护性

### 项目特色
- 🔥 **实际物联网项目** - 非 Demo
- 🔥 **多端协同** - Android + ESP32 + Cloud
- 🔥 **统一上下文** - 跨端记忆同步
- 🔥 **生产级架构** - MVVM + Repository + Room

---

## 问题排查

### Gradle 同步失败
- 检查网络连接
- 使用镜像源（阿里云 Maven）
- 确认 JDK 版本 ≥ 17

### Room 编译错误
- 确认使用 KSP（Kotlin Symbol Processing）
- 检查 Entity 字段类型

### SSE 连接失败
- 检查后端 URL 是否正确
- 确认后端支持 SSE（Content-Type: text/event-stream）
- 检查网络权限

---

## 许可证

本项目属于个人学习项目，未设置开源许可证。

---

## 联系方式

- 项目负责人: xiapiya
- 相关文档:
  - `.trellis/spec/frontend/index.md` - 前端开发规范
  - `.trellis/tasks/04-19-android-text-client/prd.md` - 详细需求文档
  - `android_v4.2_migration_summary.md` - 方案变更总结

---

**开发周期**: 预计 3 周
**当前进度**: Phase 1 完成 ✅
**下一步**: 后端联调 + SSE 测试

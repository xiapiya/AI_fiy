# Frontend Development Guidelines (V4.2 极简解耦版)

> 前端开发规范 (Android + Web监控端)
>
> **版本演进**: V4.2 Android极简重构 - 完全解耦硬件控制，专注文本对话

---

## Overview

**V4.2多端架构**:
1. **Android移动端**: 移动文本对话客户端，共享云端统一上下文
2. **Web监控端**: 管理员视角，实时查看云端处理链路和抓拍画面

---

## Android 移动端 (Kotlin) - 极简文本客户端

**技术栈**: Kotlin + OkHttp/Retrofit + SSE + Room + Material Design

**核心功能**:
- 文本对话交互（流式渲染）
- 跨端记忆同步（通过Session ID共享云端上下文）
- 历史聊天记录本地持久化（Room）
- 极简UI（纯文本气泡）

**核心原则**:
- 极致减法 - 剔除所有硬件控制逻辑
- 完全解耦 - 不依赖ESP32在线状态
- 免鉴权设计 - 硬编码Session ID
- 复用成熟库 + Kotlin协程

---

## Guidelines Index

| 指南 | 描述 | 状态 |
|------|-----|------|
| [HTTP通信客户端](#http通信客户端) | Retrofit/OkHttp使用规范 | ✅ 已填写 |
| [SSE流式渲染](#sse流式渲染) | Server-Sent Events集成 | ✅ 已填写 |
| [Session ID管理](#session-id管理) | 硬编码身份识别 | ✅ 已填写 |
| [Room数据库](#room数据库) | 本地持久化规范 | ✅ 已填写 |
| [禁止造轮子](#禁止造轮子) | 必须复用的库 | ✅ 已填写 |

---

## HTTP通信客户端

### 必须使用的库

```kotlin
// build.gradle.kts
dependencies {
    implementation("com.squareup.retrofit2:retrofit:2.9.0")
    implementation("com.squareup.retrofit2:converter-gson:2.9.0")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    implementation("com.squareup.okhttp3:okhttp-sse:4.12.0")  // SSE支持
}
```

### 文本上报接口

```kotlin
// ApiService.kt
interface ChatApiService {
    @POST("/api/v1/chat/text")
    suspend fun sendMessage(@Body request: ChatRequest): Response<ChatResponse>
}

// ChatRequest.kt
data class ChatRequest(
    val session_id: String,
    val content: String,
    val timestamp: Long = System.currentTimeMillis()
)

// ChatRepository.kt
class ChatRepository(private val api: ChatApiService) {

    companion object {
        // 硬编码Session ID - 必须与ESP32 DEVICE_ID保持一致
        const val SESSION_ID = "xiapiya_master_01"
    }

    suspend fun sendTextMessage(content: String): Result<ChatResponse> {
        return try {
            val response = api.sendMessage(
                ChatRequest(
                    session_id = SESSION_ID,
                    content = content
                )
            )
            if (response.isSuccessful && response.body() != null) {
                Result.success(response.body()!!)
            } else {
                Result.failure(Exception("发送失败: ${response.code()}"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }
}
```

---

## SSE流式渲染

### SSE客户端实现

```kotlin
// SseClient.kt
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.sse.EventSource
import okhttp3.sse.EventSourceListener
import okhttp3.sse.EventSources

class SseClient(
    private val client: OkHttpClient,
    private val baseUrl: String
) {

    fun streamChatResponse(
        sessionId: String,
        content: String,
        onChunk: (String) -> Unit,
        onComplete: () -> Unit,
        onError: (Throwable) -> Unit
    ) {
        val request = Request.Builder()
            .url("$baseUrl/api/v1/chat/stream")
            .post(createJsonBody(sessionId, content))
            .build()

        val listener = object : EventSourceListener() {
            override fun onEvent(eventSource: EventSource, id: String?, type: String?, data: String) {
                onChunk(data)  // 逐字渲染
            }

            override fun onClosed(eventSource: EventSource) {
                onComplete()
            }

            override fun onFailure(eventSource: EventSource, t: Throwable?, response: Response?) {
                onError(t ?: Exception("SSE连接失败"))
            }
        }

        EventSources.createFactory(client).newEventSource(request, listener)
    }
}
```

---

## Session ID管理

### 硬编码身份识别

为了快速迭代，Android端采用硬编码Session ID，无需登录鉴权。

```kotlin
// Constants.kt
object AppConstants {
    /**
     * 全局Session ID - 必须与ESP32固件中的DEVICE_ID保持一致
     * 用于云端索引统一上下文记忆
     */
    const val CLIENT_SESSION_ID = "xiapiya_master_01"

    /**
     * 后端API Base URL
     */
    const val API_BASE_URL = "https://yourdomain.com"
}
```

### 跨端记忆同步原理

```
┌─────────────┐
│ Android App │ ──── POST /api/v1/chat/text ────┐
└─────────────┘     session_id: xiapiya_master_01│
                                                  ▼
┌─────────────┐                           ┌──────────────┐
│  ESP32-S3   │ ──── MQTT Audio Upload ───│ FastAPI 云端 │
└─────────────┘     device_id: xiapiya... │              │
                                           │ Redis/MySQL  │
                    ┌──────────────────────│ 统一上下文   │
                    │                      └──────────────┘
                    │ 提取session_id对应的完整历史
                    ▼
            ┌──────────────┐
            │   Qwen LLM   │
            │ 多模态推理    │
            └──────────────┘
```

---

## Room数据库

### 实体定义

```kotlin
// ChatMessage.kt
import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "chat_messages")
data class ChatMessage(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val sessionId: String,           // Session ID标识
    val content: String,              // 消息文本内容
    val isFromUser: Boolean,         // true=用户发送, false=AI回复
    val timestamp: Long = System.currentTimeMillis()
)

// ChatDao.kt
import androidx.room.*
import kotlinx.coroutines.flow.Flow

@Dao
interface ChatDao {
    @Query("SELECT * FROM chat_messages WHERE sessionId = :sessionId ORDER BY timestamp ASC")
    fun getMessages(sessionId: String): Flow<List<ChatMessage>>

    @Insert
    suspend fun insert(message: ChatMessage)

    @Query("DELETE FROM chat_messages WHERE sessionId = :sessionId")
    suspend fun clearHistory(sessionId: String)
}

// AppDatabase.kt
@Database(entities = [ChatMessage::class], version = 1)
abstract class AppDatabase : RoomDatabase() {
    abstract fun chatDao(): ChatDao
}
```

---

## Web 监控端 (V4.0新增)

**技术栈**: HTML/JS + WebSocket Secure (WSS) + Chart.js
**部署**: Nginx静态文件托管
**权限**: 只读权限,不下发控制指令

**核心功能**:
1. WSS连接EMQX,订阅`emqx/system/logs`
2. 实时显示云端处理链路日志
3. 展示抓拍图片记录
4. 耗时统计图表 (全链路延迟分析)

详见: [web-monitor.md](./web-monitor.md)

---

## 禁止造轮子

### 必须复用的库

| 功能 | 必须使用的库 | 禁止行为 |
|------|------------|---------|
| HTTP客户端 | Retrofit + OkHttp | ❌ 不要用HttpURLConnection |
| SSE流式 | OkHttp-SSE | ❌ 不要自己解析SSE协议 |
| 数据库 | Room | ❌ 不要用SQLite原始API |
| 协程 | Kotlin Coroutines | ❌ 不要用Thread/AsyncTask |
| JSON解析 | Gson/Moshi | ❌ 不要手动解析JSON |
| WebSocket (Web端) | 原生WebSocket API | ❌ 不要自己实现WSS握手 |

---

## 核心原则

### ✅ DO

- 使用 Retrofit + OkHttp 处理HTTP通信
- 使用 SSE 实现流式文本渲染
- 使用 Room 持久化聊天记录
- 使用 Kotlin 协程管理异步
- 硬编码 Session ID (快速迭代，无需登录)
- 使用 MVVM 架构 (ViewModel + Repository)
- 使用 codex MCP 审查代码

### ❌ DON'T

- ❌ 不要造轮子
- ❌ 不要实现账号登录系统（当前阶段）
- ❌ 不要添加硬件控制功能（与ESP32完全解耦）
- ❌ 不要监听ESP32状态（独立对话模式）
- ❌ 不要使用废弃的库
- ❌ Web端不要尝试下发控制指令 (只读权限)

---

## UI/UX设计规范

### 界面布局
```
┌─────────────────────────────────┐
│  ⬅  xiapiya 情感陪伴智能体        │ ← 顶部导航栏
├─────────────────────────────────┤
│                                 │
│  ┌───────────────────────┐     │
│  │ AI: 你好，主人！       │     │ ← 左侧气泡 (AI回复)
│  └───────────────────────┘     │
│                                 │
│           ┌─────────────────┐  │
│           │ 我: 在吗？      │  │ ← 右侧气泡 (用户输入)
│           └─────────────────┘  │
│                                 │
│  ┌───────────────────────┐     │
│  │ AI: 在的，有什么需...  │     │ ← 流式渲染中
│  └───────────────────────┘     │
│                                 │
├─────────────────────────────────┤
│  ┌─────────────────────┐  [📤] │ ← 底部输入区
│  │ 输入消息...          │       │
│  └─────────────────────┘       │
└─────────────────────────────────┘
```

### 极简设计原则
- 无多余按钮（不显示"拍照"、"静音"等硬件控制）
- 纯文本交互（无表情、无图片）
- 流式渲染（逐字显示AI回复）
- 简洁配色（Material Design 3）

---

**总结**: 极简解耦 + 纯文本对话 + 跨端记忆同步 + 流式体验

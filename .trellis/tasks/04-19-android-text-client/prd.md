# Android文本对话客户端开发 (V4.2 极简解耦版)

## 目标
开发一个极简的Android移动端文本对话客户端，作为xiapiya情感陪伴智能体的移动数字分身，通过云端统一上下文实现跨端记忆同步，完全解耦硬件控制逻辑。

## 开发类型
**前端开发** (Frontend - Android/Kotlin)

---

## 核心理念

### 极致减法
剔除所有非核心的硬件控制逻辑（远程拍照、静音开关、设备状态监控等），专注于纯文本对话体验。

### 完全解耦
- Android端不依赖ESP32在线状态
- 对话请求不触发桌面硬件动作
- 独立的对话模式，仅通过Session ID共享云端上下文

### 快速迭代
- 免鉴权设计（硬编码Session ID）
- 使用成熟库（Retrofit + Room + SSE）
- MVVM架构保证代码质量

---

## 功能需求

### 核心功能 (P0)
- [ ] 文本输入与发送
- [ ] AI回复流式渲染（SSE）
- [ ] 聊天历史本地持久化（Room）
- [ ] 极简UI（Material Design 3）
- [ ] 跨端记忆同步（通过Session ID）

### 辅助功能 (P1)
- [ ] 离线查看历史记录
- [ ] 网络错误提示
- [ ] 清空对话历史
- [ ] 深色模式支持（可选）

### 未来扩展 (P2)
- [ ] 多设备管理（管理多个Session ID）
- [ ] 推送通知
- [ ] 账号登录系统

---

## 技术需求

### 项目结构
```
xiapiya-android/
├── app/
│   ├── src/main/
│   │   ├── java/com/xiapiya/companion/
│   │   │   ├── data/              # 数据层
│   │   │   │   ├── local/         # Room数据库
│   │   │   │   │   ├── ChatMessage.kt
│   │   │   │   │   ├── ChatDao.kt
│   │   │   │   │   └── AppDatabase.kt
│   │   │   │   ├── remote/        # HTTP/SSE客户端
│   │   │   │   │   ├── ChatApiService.kt
│   │   │   │   │   ├── SseClient.kt
│   │   │   │   │   └── models/
│   │   │   │   │       ├── ChatRequest.kt
│   │   │   │   │       └── ChatResponse.kt
│   │   │   │   └── repository/
│   │   │   │       └── ChatRepository.kt
│   │   │   ├── domain/            # 业务逻辑层
│   │   │   │   ├── model/
│   │   │   │   │   └── Message.kt
│   │   │   │   └── usecase/
│   │   │   │       ├── SendMessageUseCase.kt
│   │   │   │       └── GetMessagesUseCase.kt
│   │   │   ├── presentation/      # UI层
│   │   │   │   ├── chat/
│   │   │   │   │   ├── ChatActivity.kt
│   │   │   │   │   ├── ChatViewModel.kt
│   │   │   │   │   └── ChatAdapter.kt
│   │   │   │   └── common/
│   │   │   │       └── ViewModelFactory.kt
│   │   │   └── util/
│   │   │       └── Constants.kt
│   │   └── res/
│   │       ├── layout/
│   │       │   ├── activity_chat.xml
│   │       │   ├── item_message_user.xml
│   │       │   └── item_message_ai.xml
│   │       └── values/
│   │           ├── colors.xml
│   │           └── strings.xml
├── build.gradle.kts
└── README.md
```

### 依赖库
```kotlin
// build.gradle.kts (app)
dependencies {
    // Kotlin
    implementation("org.jetbrains.kotlin:kotlin-stdlib:1.9.22")

    // 协程
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3")

    // HTTP客户端
    implementation("com.squareup.retrofit2:retrofit:2.9.0")
    implementation("com.squareup.retrofit2:converter-gson:2.9.0")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
    implementation("com.squareup.okhttp3:okhttp-sse:4.12.0")

    // Room数据库
    implementation("androidx.room:room-runtime:2.6.1")
    implementation("androidx.room:room-ktx:2.6.1")
    kapt("androidx.room:room-compiler:2.6.1")

    // ViewModel & LiveData
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-livedata-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")

    // RecyclerView
    implementation("androidx.recyclerview:recyclerview:1.3.2")

    // Material Design
    implementation("com.google.android.material:material:1.11.0")

    // JSON解析
    implementation("com.google.code.gson:gson:2.10.1")

    // 测试
    testImplementation("junit:junit:4.13.2")
    testImplementation("io.mockk:mockk:1.13.9")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}
```

---

## 核心模块设计

### 1. Session ID管理
```kotlin
// util/Constants.kt
object AppConstants {
    /**
     * 硬编码Session ID
     * 必须与ESP32固件中的DEVICE_ID保持一致: xiapiya_s3_001
     * 云端通过此ID索引统一上下文记忆
     */
    const val CLIENT_SESSION_ID = "xiapiya_master_01"

    /**
     * 后端API Base URL
     */
    const val API_BASE_URL = "https://yourdomain.com"
}
```

### 2. HTTP接口定义
```kotlin
// data/remote/ChatApiService.kt
interface ChatApiService {
    /**
     * 发送文本消息到云端
     */
    @POST("/api/v1/chat/text")
    suspend fun sendMessage(@Body request: ChatRequest): Response<ChatResponse>
}

// data/remote/models/ChatRequest.kt
data class ChatRequest(
    val session_id: String,
    val content: String,
    val timestamp: Long = System.currentTimeMillis()
)

// data/remote/models/ChatResponse.kt
data class ChatResponse(
    val reply: String,          // AI回复文本
    val emotion: String?,       // 可选：情绪标签
    val timestamp: Long
)
```

### 3. SSE流式渲染客户端
```kotlin
// data/remote/SseClient.kt
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.sse.EventSource
import okhttp3.sse.EventSourceListener
import okhttp3.sse.EventSources
import okhttp3.RequestBody.Companion.toRequestBody

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
        val json = """
            {
                "session_id": "$sessionId",
                "content": "$content",
                "timestamp": ${System.currentTimeMillis()}
            }
        """.trimIndent()

        val requestBody = json.toRequestBody("application/json".toMediaType())

        val request = Request.Builder()
            .url("$baseUrl/api/v1/chat/stream")
            .post(requestBody)
            .build()

        val listener = object : EventSourceListener() {
            override fun onEvent(
                eventSource: EventSource,
                id: String?,
                type: String?,
                data: String
            ) {
                // 逐字流式渲染
                onChunk(data)
            }

            override fun onClosed(eventSource: EventSource) {
                onComplete()
            }

            override fun onFailure(
                eventSource: EventSource,
                t: Throwable?,
                response: okhttp3.Response?
            ) {
                onError(t ?: Exception("SSE连接失败"))
            }
        }

        EventSources.createFactory(client).newEventSource(request, listener)
    }
}
```

### 4. Room数据库
```kotlin
// data/local/ChatMessage.kt
@Entity(tableName = "chat_messages")
data class ChatMessage(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    val sessionId: String,
    val content: String,
    val isFromUser: Boolean,
    val timestamp: Long
)

// data/local/ChatDao.kt
@Dao
interface ChatDao {
    @Query("SELECT * FROM chat_messages WHERE sessionId = :sessionId ORDER BY timestamp ASC")
    fun getMessagesFlow(sessionId: String): Flow<List<ChatMessage>>

    @Insert
    suspend fun insert(message: ChatMessage)

    @Query("DELETE FROM chat_messages WHERE sessionId = :sessionId")
    suspend fun clearHistory(sessionId: String)
}

// data/local/AppDatabase.kt
@Database(entities = [ChatMessage::class], version = 1)
abstract class AppDatabase : RoomDatabase() {
    abstract fun chatDao(): ChatDao

    companion object {
        @Volatile
        private var INSTANCE: AppDatabase? = null

        fun getDatabase(context: Context): AppDatabase {
            return INSTANCE ?: synchronized(this) {
                val instance = Room.databaseBuilder(
                    context.applicationContext,
                    AppDatabase::class.java,
                    "xiapiya_chat_db"
                ).build()
                INSTANCE = instance
                instance
            }
        }
    }
}
```

### 5. Repository模式
```kotlin
// data/repository/ChatRepository.kt
class ChatRepository(
    private val chatDao: ChatDao,
    private val apiService: ChatApiService,
    private val sseClient: SseClient
) {

    fun getMessages(sessionId: String): Flow<List<ChatMessage>> {
        return chatDao.getMessagesFlow(sessionId)
    }

    suspend fun sendMessage(
        sessionId: String,
        content: String,
        onStreamChunk: (String) -> Unit,
        onComplete: () -> Unit,
        onError: (Throwable) -> Unit
    ) {
        // 1. 保存用户消息到本地
        chatDao.insert(
            ChatMessage(
                sessionId = sessionId,
                content = content,
                isFromUser = true,
                timestamp = System.currentTimeMillis()
            )
        )

        // 2. SSE流式接收AI回复
        var fullReply = ""
        sseClient.streamChatResponse(
            sessionId = sessionId,
            content = content,
            onChunk = { chunk ->
                fullReply += chunk
                onStreamChunk(chunk)
            },
            onComplete = {
                // 3. 保存完整AI回复到本地
                GlobalScope.launch {
                    chatDao.insert(
                        ChatMessage(
                            sessionId = sessionId,
                            content = fullReply,
                            isFromUser = false,
                            timestamp = System.currentTimeMillis()
                        )
                    )
                }
                onComplete()
            },
            onError = onError
        )
    }

    suspend fun clearHistory(sessionId: String) {
        chatDao.clearHistory(sessionId)
    }
}
```

### 6. ViewModel
```kotlin
// presentation/chat/ChatViewModel.kt
class ChatViewModel(
    private val repository: ChatRepository
) : ViewModel() {

    private val sessionId = AppConstants.CLIENT_SESSION_ID

    val messages: LiveData<List<ChatMessage>> = repository
        .getMessages(sessionId)
        .asLiveData()

    private val _streamingText = MutableLiveData<String>()
    val streamingText: LiveData<String> = _streamingText

    fun sendMessage(content: String) {
        viewModelScope.launch {
            _streamingText.value = "" // 重置流式文本

            repository.sendMessage(
                sessionId = sessionId,
                content = content,
                onStreamChunk = { chunk ->
                    _streamingText.postValue((_streamingText.value ?: "") + chunk)
                },
                onComplete = {
                    _streamingText.postValue("")
                },
                onError = { error ->
                    // 错误处理
                    Log.e("ChatViewModel", "发送失败", error)
                }
            )
        }
    }

    fun clearHistory() {
        viewModelScope.launch {
            repository.clearHistory(sessionId)
        }
    }
}
```

---

## UI/UX设计

### 聊天界面
```xml
<!-- res/layout/activity_chat.xml -->
<androidx.constraintlayout.widget.ConstraintLayout>

    <!-- 顶部导航栏 -->
    <com.google.android.material.appbar.MaterialToolbar
        android:id="@+id/toolbar"
        app:title="xiapiya 情感陪伴智能体" />

    <!-- 消息列表 -->
    <androidx.recyclerview.widget.RecyclerView
        android:id="@+id/recyclerView"
        app:layoutManager="androidx.recyclerview.widget.LinearLayoutManager" />

    <!-- 底部输入区 -->
    <LinearLayout
        android:id="@+id/inputLayout"
        android:orientation="horizontal">

        <com.google.android.material.textfield.TextInputEditText
            android:id="@+id/inputEditText"
            android:hint="输入消息..." />

        <com.google.android.material.button.MaterialButton
            android:id="@+id/sendButton"
            android:text="发送" />

    </LinearLayout>

</androidx.constraintlayout.widget.ConstraintLayout>
```

### 消息气泡样式
- 用户消息：右对齐，蓝色背景
- AI回复：左对齐，灰色背景
- 流式渲染：实时追加文本到当前气泡

---

## 验收标准

### 功能验收
- [ ] 文本输入并发送到云端
- [ ] 接收AI回复并流式渲染
- [ ] 聊天记录本地持久化
- [ ] 离线查看历史记录
- [ ] 跨端上下文同步（通过Session ID）

### 性能验收
- [ ] 流式渲染无卡顿
- [ ] Room数据库无内存泄漏
- [ ] 网络请求超时处理
- [ ] 弱网环境正常降级

### 代码质量
- [ ] MVVM架构规范
- [ ] Kotlin协程正确使用
- [ ] 无内存泄漏
- [ ] 单元测试覆盖率>50%

---

## 技术风险

### 风险1: SSE连接不稳定
- **影响**: 流式渲染中断
- **应对**: 实现重连机制，超时降级为HTTP轮询

### 风险2: Session ID硬编码维护困难
- **影响**: 多设备管理复杂
- **应对**: 后续迭代引入账号系统

### 风险3: 云端接口未就绪
- **影响**: 无法联调
- **应对**: 先用Mock数据开发UI和数据库层

---

## 实施步骤

### Phase 1: 项目框架 (3天)
- [ ] 创建Android Studio项目
- [ ] 配置Gradle依赖
- [ ] 创建MVVM包结构
- [ ] 实现Constants和基础配置

### Phase 2: Room数据库 (2天)
- [ ] 定义Entity、Dao、Database
- [ ] 实现Repository基础方法
- [ ] 单元测试数据库操作

### Phase 3: HTTP/SSE客户端 (3天)
- [ ] 实现Retrofit接口
- [ ] 实现SSE流式客户端
- [ ] Mock测试网络层

### Phase 4: UI实现 (4天)
- [ ] 聊天界面布局
- [ ] RecyclerView Adapter
- [ ] 流式文本渲染
- [ ] Material Design样式

### Phase 5: ViewModel集成 (2天)
- [ ] 实现ChatViewModel
- [ ] 连接Repository和UI
- [ ] LiveData/Flow集成

### Phase 6: 联调测试 (3天)
- [ ] 与后端API联调
- [ ] 跨端上下文验证
- [ ] 弱网测试
- [ ] 性能优化

### Phase 7: 优化交付 (3天)
- [ ] 代码审查
- [ ] 单元测试补充
- [ ] README文档
- [ ] APK打包

**总计**: 20天（约3周）

---

## 后端依赖

### 需要后端提供的接口

#### 1. 文本对话接口
```
POST /api/v1/chat/text
Content-Type: application/json

Request:
{
    "session_id": "xiapiya_master_01",
    "content": "主人输入的文本",
    "timestamp": 1713514200
}

Response:
{
    "reply": "AI生成的回复文本",
    "emotion": "happy",
    "timestamp": 1713514205
}
```

#### 2. SSE流式接口（可选）
```
POST /api/v1/chat/stream
Content-Type: application/json
Accept: text/event-stream

Request: (同上)

Response:
event: message
data: 你

event: message
data: 好

event: message
data: ，

event: done
data: {"total_tokens": 42}
```

### 云端上下文管理
后端需要实现：
- Redis/MySQL存储session_id对应的对话历史
- 接收到请求时，提取完整上下文（包括ESP32的语音对话）
- 拼接后提交给Qwen LLM
- 返回回复并更新历史

---

## 参考规范文档

- `.trellis/spec/frontend/index.md` - 前端开发规范（已更新V4.2）
- MVVM架构最佳实践
- Material Design 3指南
- Kotlin协程官方文档

---

## 面试展示价值

### 技术栈覆盖
- ✅ Kotlin + 协程
- ✅ MVVM架构
- ✅ Jetpack Room
- ✅ Retrofit + OkHttp
- ✅ SSE流式通信
- ✅ Material Design 3
- ✅ 单元测试

### 项目亮点
- 🔥 实际物联网项目（非Demo）
- 🔥 跨端协同（Android + ESP32 + Cloud）
- 🔥 流式渲染（实时体验）
- 🔥 Clean Architecture（分层设计）
- 🔥 生产级代码（规范+测试）

---

**预期成果**: 3周内完成可展示的Android文本对话客户端，适合面试展示。

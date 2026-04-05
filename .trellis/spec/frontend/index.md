# Android Development Guidelines (Kotlin)

> Android客户端开发规范

---

## Overview

**技术栈**: Kotlin + MQTT (HiveMQ) + Room + Foreground Service + Material Design

本项目Android端为**情感陪伴智能体的移动伴侣端**,提供:
- 实时监控ESP32设备状态
- 远程语音发送
- 远程拍照指令
- 历史聊天记录本地持久化(Room)
- "静音硬件"开关(防止多设备冲突)

**核心原则**: 复用成熟库 + Kotlin协程 + 后台保活

---

## Guidelines Index

| 指南 | 描述 | 状态 |
|------|-----|------|
| [MQTT客户端](#mqtt客户端) | HiveMQ MQTT使用规范 | ✅ 已填写 |
| [Room数据库](#room数据库) | 本地持久化规范 | ✅ 已填写 |
| [后台保活](#后台保活) | Foreground Service保活 | ✅ 已填写 |
| [禁止造轮子](#禁止造轮子) | 必须复用的库 | ✅ 已填写 |

---

## MQTT客户端

### 必须使用的库

```kotlin
// build.gradle.kts
dependencies {
    implementation("com.hivemq:hivemq-mqtt-client:1.3.3")  // 推荐
    // 或 implementation("org.eclipse.paho:org.eclipse.paho.client.mqttv3:1.2.5")
}
```

### 示例代码

```kotlin
// MQTTManager.kt
import com.hivemq.client.mqtt.mqtt5.Mqtt5AsyncClient
import kotlinx.coroutines.flow.MutableStateFlow

class MQTTManager {
    private lateinit var client: Mqtt5AsyncClient

    suspend fun connect(broker: String, deviceId: String) {
        client = Mqtt5AsyncClient.builder()
            .serverHost(broker)
            .serverPort(1883)
            .identifier("android_$deviceId")
            .build()

        client.connectWith().send().await()

        // 订阅主题
        client.subscribeWith()
            .topicFilter("emqx/esp32/$deviceId/ui")
            .callback { publish ->
                val payload = String(publish.payloadAsBytes)
                // 处理UI更新
            }
            .send()
    }

    fun publishAudio(deviceId: String, audioBase64: String) {
        client.publishWith()
            .topic("emqx/esp32/$deviceId/audio")
            .payload(audioBase64.toByteArray())
            .send()
    }
}
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
    val deviceId: String,
    val text: String,
    val emotion: String,  // happy/sad/comfort
    val isFromUser: Boolean,
    val timestamp: Long = System.currentTimeMillis()
)

// ChatDao.kt
import androidx.room.*
import kotlinx.coroutines.flow.Flow

@Dao
interface ChatDao {
    @Query("SELECT * FROM chat_messages WHERE deviceId = :deviceId ORDER BY timestamp DESC")
    fun getMessages(deviceId: String): Flow<List<ChatMessage>>

    @Insert
    suspend fun insert(message: ChatMessage)
}
```

---

## 后台保活

### Foreground Service

```kotlin
// MQTTService.kt
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.os.Build

class MQTTService : Service() {
    override fun onCreate() {
        super.onCreate()

        // 创建Notification Channel
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                "mqtt_service",
                "MQTT连接",
                NotificationManager.IMPORTANCE_LOW
            )
            getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
        }

        // 前台服务Notification
        val notification = NotificationCompat.Builder(this, "mqtt_service")
            .setContentTitle("情感陪伴智能体")
            .setContentText("MQTT已连接")
            .setSmallIcon(R.drawable.ic_notification)
            .build()

        startForeground(1, notification)
    }
}
```

---

## 禁止造轮子

### 必须复用的库

| 功能 | 必须使用的库 | 禁止行为 |
|------|------------|---------|
| MQTT客户端 | HiveMQ MQTT Client | ❌ 不要自己写MQTT协议 |
| 数据库 | Room | ❌ 不要用SQLite原始API |
| 网络请求 | Retrofit/OkHttp | ❌ 不要用HttpURLConnection |
| 协程 | Kotlin Coroutines | ❌ 不要用Thread/AsyncTask |

---

## 核心原则

### ✅ DO

- 使用 HiveMQ MQTT Client
- 使用 Room 持久化
- 使用 Foreground Service 保活
- 使用 Kotlin 协程管理异步
- 使用 codex MCP 审查代码

### ❌ DON'T

- ❌ 不要造轮子
- ❌ 不要使用废弃的Paho Android Service
- ❌ 不要忘记申请 FOREGROUND_SERVICE 权限

---

**总结**: 复用成熟库 + 协程异步 + 后台保活

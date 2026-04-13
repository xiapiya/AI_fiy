# Frontend Development Guidelines (V4.0 多端协同版)

> 前端开发规范 (Android + Web监控端)
>
> **版本演进**: V4.0新增Web监控端(WSS实时可视化) + Android远程控制增强

---

## Overview

**V4.0多端架构**:
1. **Android移动端**: 用户随身伴侣,远程控制宿舍/桌面设备
2. **Web监控端**: 管理员视角,实时查看云端处理链路和抓拍画面

---

## Android 移动端 (Kotlin)

**技术栈**: Kotlin + HiveMQ MQTT + Room + Foreground Service + Material Design

**核心功能**:
- 实时监控ESP32设备状态
- 远程语音发送
- **远程拍照指令** (V4.0新增)
- **静音硬件开关** (V4.0新增,防止多设备冲突)
- 历史聊天记录+图片本地持久化(Room)

**核心原则**: 复用成熟库 + Kotlin协程 + Foreground Service保活

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

## 远程控制功能 (V4.0新增)

### 远程拍照指令

```kotlin
// RemoteControlManager.kt
class RemoteControlManager(private val mqttClient: Mqtt5AsyncClient) {

    /**
     * 发送远程拍照指令
     * 下发到主题: emqx/esp32/{deviceId}/control
     */
    fun requestCapture(deviceId: String) {
        val payload = JSONObject().apply {
            put("action", "capture")
            put("timestamp", System.currentTimeMillis())
        }.toString()

        mqttClient.publishWith()
            .topic("emqx/esp32/$deviceId/control")
            .payload(payload.toByteArray())
            .send()

        Log.d(TAG, "已发送远程拍照指令到设备: $deviceId")
    }

    /**
     * 静音硬件开关
     * 防止用户在外用App对话时,宿舍桌面的硬件也响应麦克风
     */
    fun muteMicrophone(deviceId: String, muted: Boolean) {
        val payload = JSONObject().apply {
            put("action", "mute")
            put("muted", muted)
        }.toString()

        mqttClient.publishWith()
            .topic("emqx/esp32/$deviceId/control")
            .payload(payload.toByteArray())
            .send()
    }
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
| MQTT客户端 | HiveMQ MQTT Client | ❌ 不要自己写MQTT协议 |
| 数据库 | Room | ❌ 不要用SQLite原始API |
| 网络请求 | Retrofit/OkHttp | ❌ 不要用HttpURLConnection |
| 协程 | Kotlin Coroutines | ❌ 不要用Thread/AsyncTask |
| WebSocket (Web端) | 原生WebSocket API | ❌ 不要自己实现WSS握手 |

---

## 核心原则

### ✅ DO

- 使用 HiveMQ MQTT Client (Android)
- 使用 Room 持久化聊天记录和图片
- 使用 Foreground Service 保活
- 使用 Kotlin 协程管理异步
- Web端使用WSS (而非WS) 连接EMQX
- 使用 codex MCP 审查代码

### ❌ DON'T

- ❌ 不要造轮子
- ❌ 不要使用废弃的Paho Android Service
- ❌ 不要忘记申请 FOREGROUND_SERVICE 权限
- ❌ Web端不要尝试下发控制指令 (只读权限)

---

**总结**: 复用成熟库 + 协程异步 + 后台保活 + 多端协同

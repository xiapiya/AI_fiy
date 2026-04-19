package com.xiapiya.companion.data.local

import androidx.room.Entity
import androidx.room.PrimaryKey

/**
 * 聊天消息实体
 *
 * 用于本地持久化存储对话历史
 */
@Entity(tableName = "chat_messages")
data class ChatMessage(
    @PrimaryKey(autoGenerate = true)
    val id: Long = 0,

    /**
     * Session ID - 关联到统一上下文
     */
    val sessionId: String,

    /**
     * 消息文本内容
     */
    val content: String,

    /**
     * 是否为用户发送的消息
     * true - 用户发送
     * false - AI 回复
     */
    val isFromUser: Boolean,

    /**
     * 消息时间戳（毫秒）
     */
    val timestamp: Long
)

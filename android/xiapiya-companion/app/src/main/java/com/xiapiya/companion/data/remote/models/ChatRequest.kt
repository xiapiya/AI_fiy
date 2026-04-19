package com.xiapiya.companion.data.remote.models

import com.google.gson.annotations.SerializedName

/**
 * 聊天请求数据模型
 */
data class ChatRequest(
    @SerializedName("session_id")
    val sessionId: String,

    @SerializedName("content")
    val content: String,

    @SerializedName("timestamp")
    val timestamp: Long = System.currentTimeMillis()
)

package com.xiapiya.companion.data.remote.models

import com.google.gson.annotations.SerializedName

/**
 * 聊天响应数据模型
 */
data class ChatResponse(
    @SerializedName("reply")
    val reply: String,

    @SerializedName("emotion")
    val emotion: String? = null,

    @SerializedName("timestamp")
    val timestamp: Long
)

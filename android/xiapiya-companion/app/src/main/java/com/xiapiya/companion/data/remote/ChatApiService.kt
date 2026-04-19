package com.xiapiya.companion.data.remote

import com.xiapiya.companion.data.remote.models.ChatRequest
import com.xiapiya.companion.data.remote.models.ChatResponse
import retrofit2.Response
import retrofit2.http.Body
import retrofit2.http.POST

/**
 * 聊天 API 接口定义
 */
interface ChatApiService {

    /**
     * 发送文本消息到云端（同步响应）
     */
    @POST("/api/v1/chat/text")
    suspend fun sendMessage(@Body request: ChatRequest): Response<ChatResponse>

    // 注意：SSE流式接口不通过 Retrofit，使用 OkHttp SSE 客户端
    // 详见 SseClient.kt
}

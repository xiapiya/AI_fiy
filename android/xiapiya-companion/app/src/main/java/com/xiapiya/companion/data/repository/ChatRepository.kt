package com.xiapiya.companion.data.repository

import android.util.Log
import com.xiapiya.companion.data.local.ChatDao
import com.xiapiya.companion.data.local.ChatMessage
import com.xiapiya.companion.data.remote.ChatApiService
import com.xiapiya.companion.data.remote.SseClient
import com.xiapiya.companion.data.remote.models.ChatRequest
import com.xiapiya.companion.util.AppConstants
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.withContext
import okhttp3.sse.EventSource

/**
 * 聊天数据仓库
 *
 * 统一管理本地数据库和远程 API 调用
 */
class ChatRepository(
    private val chatDao: ChatDao,
    private val apiService: ChatApiService,
    private val sseClient: SseClient
) {

    companion object {
        private const val TAG = "${AppConstants.LOG_TAG}.ChatRepo"
    }

    /**
     * 获取指定 Session 的所有消息（Flow 自动更新）
     */
    fun getMessages(sessionId: String): Flow<List<ChatMessage>> {
        return chatDao.getMessagesFlow(sessionId)
    }

    /**
     * 发送消息到云端（同步响应模式）
     *
     * @return Result<String> 成功返回 AI 回复文本，失败返回异常
     */
    suspend fun sendMessageSync(
        sessionId: String,
        content: String
    ): Result<String> = withContext(Dispatchers.IO) {
        try {
            // 1. 保存用户消息到本地
            val userMessage = ChatMessage(
                sessionId = sessionId,
                content = content,
                isFromUser = true,
                timestamp = System.currentTimeMillis()
            )
            chatDao.insert(userMessage)

            // 2. 调用后端 API
            val response = apiService.sendMessage(
                ChatRequest(
                    sessionId = sessionId,
                    content = content
                )
            )

            if (response.isSuccessful && response.body() != null) {
                val chatResponse = response.body()!!
                val aiReply = chatResponse.reply

                // 3. 保存 AI 回复到本地
                val aiMessage = ChatMessage(
                    sessionId = sessionId,
                    content = aiReply,
                    isFromUser = false,
                    timestamp = chatResponse.timestamp
                )
                chatDao.insert(aiMessage)

                Result.success(aiReply)
            } else {
                Result.failure(Exception("发送失败: ${response.code()}"))
            }
        } catch (e: Exception) {
            Log.e(TAG, "发送消息失败", e)
            Result.failure(e)
        }
    }

    /**
     * 发送消息到云端（SSE 流式响应模式）
     *
     * @param onStreamChunk 每接收到一个文本块时的回调
     * @param onComplete 接收完成时的回调
     * @param onError 错误时的回调
     * @return EventSource 可用于取消连接
     */
    suspend fun sendMessageStream(
        sessionId: String,
        content: String,
        onStreamChunk: (String) -> Unit,
        onComplete: suspend (String) -> Unit,
        onError: (Throwable) -> Unit
    ): EventSource = withContext(Dispatchers.IO) {

        // 1. 保存用户消息到本地
        val userMessage = ChatMessage(
            sessionId = sessionId,
            content = content,
            isFromUser = true,
            timestamp = System.currentTimeMillis()
        )
        chatDao.insert(userMessage)

        // 2. SSE 流式接收 AI 回复
        var fullReply = ""

        sseClient.streamChatResponse(
            sessionId = sessionId,
            content = content,
            onChunk = { chunk ->
                fullReply += chunk
                onStreamChunk(chunk)
            },
            onComplete = {
                // 3. 保存完整 AI 回复到本地
                kotlinx.coroutines.GlobalScope.launch(Dispatchers.IO) {
                    val aiMessage = ChatMessage(
                        sessionId = sessionId,
                        content = fullReply,
                        isFromUser = false,
                        timestamp = System.currentTimeMillis()
                    )
                    chatDao.insert(aiMessage)
                    onComplete(fullReply)
                }
            },
            onError = onError
        )
    }

    /**
     * 清空指定 Session 的历史记录
     */
    suspend fun clearHistory(sessionId: String) = withContext(Dispatchers.IO) {
        chatDao.clearHistory(sessionId)
    }

    /**
     * 获取消息数量
     */
    suspend fun getMessageCount(sessionId: String): Int = withContext(Dispatchers.IO) {
        chatDao.getMessageCount(sessionId)
    }
}

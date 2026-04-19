package com.xiapiya.companion.data.remote

import android.util.Log
import com.google.gson.Gson
import com.xiapiya.companion.data.remote.models.ChatRequest
import com.xiapiya.companion.util.AppConstants
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import okhttp3.sse.EventSource
import okhttp3.sse.EventSourceListener
import okhttp3.sse.EventSources

/**
 * SSE 流式客户端
 *
 * 用于接收 AI 回复的流式文本渲染
 */
class SseClient(
    private val client: OkHttpClient,
    private val baseUrl: String,
    private val gson: Gson
) {

    companion object {
        private const val TAG = "${AppConstants.LOG_TAG}.SseClient"
    }

    /**
     * 流式接收聊天响应
     *
     * @param sessionId Session ID
     * @param content 用户输入文本
     * @param onChunk 每接收到一个文本块时的回调
     * @param onComplete 接收完成时的回调
     * @param onError 错误时的回调
     */
    fun streamChatResponse(
        sessionId: String,
        content: String,
        onChunk: (String) -> Unit,
        onComplete: () -> Unit,
        onError: (Throwable) -> Unit
    ): EventSource {

        val chatRequest = ChatRequest(
            sessionId = sessionId,
            content = content
        )

        val json = gson.toJson(chatRequest)
        val requestBody = json.toRequestBody("application/json".toMediaType())

        val request = Request.Builder()
            .url("$baseUrl/api/v1/chat/stream")
            .post(requestBody)
            .build()

        val listener = object : EventSourceListener() {

            override fun onOpen(eventSource: EventSource, response: Response) {
                Log.d(TAG, "SSE 连接已建立")
            }

            override fun onEvent(
                eventSource: EventSource,
                id: String?,
                type: String?,
                data: String
            ) {
                Log.d(TAG, "SSE 事件: type=$type, data=$data")

                when (type) {
                    "message" -> {
                        // 逐字流式渲染
                        onChunk(data)
                    }
                    "done" -> {
                        // 接收完成
                        Log.d(TAG, "SSE 流式接收完成")
                        eventSource.cancel()
                        onComplete()
                    }
                    else -> {
                        // 默认处理为消息块
                        onChunk(data)
                    }
                }
            }

            override fun onClosed(eventSource: EventSource) {
                Log.d(TAG, "SSE 连接已关闭")
                onComplete()
            }

            override fun onFailure(
                eventSource: EventSource,
                t: Throwable?,
                response: Response?
            ) {
                Log.e(TAG, "SSE 连接失败", t)
                onError(t ?: Exception("SSE 连接失败"))
            }
        }

        return EventSources.createFactory(client).newEventSource(request, listener)
    }

    /**
     * 取消 SSE 连接
     */
    fun cancel(eventSource: EventSource) {
        eventSource.cancel()
    }
}

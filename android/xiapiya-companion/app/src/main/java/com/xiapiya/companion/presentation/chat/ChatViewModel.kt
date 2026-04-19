package com.xiapiya.companion.presentation.chat

import android.util.Log
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.asLiveData
import androidx.lifecycle.viewModelScope
import com.xiapiya.companion.data.local.ChatMessage
import com.xiapiya.companion.data.repository.ChatRepository
import com.xiapiya.companion.util.AppConstants
import kotlinx.coroutines.launch
import okhttp3.sse.EventSource

/**
 * 聊天 ViewModel
 *
 * 管理聊天界面的数据和业务逻辑
 */
class ChatViewModel(
    private val repository: ChatRepository
) : ViewModel() {

    companion object {
        private const val TAG = "${AppConstants.LOG_TAG}.ChatVM"
    }

    private val sessionId = AppConstants.CLIENT_SESSION_ID

    // 消息列表（自动更新）
    val messages: LiveData<List<ChatMessage>> = repository
        .getMessages(sessionId)
        .asLiveData()

    // 流式渲染文本
    private val _streamingText = MutableLiveData<String>()
    val streamingText: LiveData<String> = _streamingText

    // 是否正在发送
    private val _isSending = MutableLiveData<Boolean>()
    val isSending: LiveData<Boolean> = _isSending

    // 错误消息
    private val _errorMessage = MutableLiveData<String>()
    val errorMessage: LiveData<String> = _errorMessage

    // SSE EventSource（用于取消）
    private var currentEventSource: EventSource? = null

    /**
     * 发送消息（SSE 流式模式）
     */
    fun sendMessage(content: String) {
        if (content.isBlank()) {
            _errorMessage.value = "消息不能为空"
            return
        }

        if (_isSending.value == true) {
            _errorMessage.value = "正在发送中，请稍候..."
            return
        }

        viewModelScope.launch {
            try {
                _isSending.value = true
                _streamingText.value = "" // 重置流式文本

                currentEventSource = repository.sendMessageStream(
                    sessionId = sessionId,
                    content = content,
                    onStreamChunk = { chunk ->
                        // 逐字追加到流式文本
                        _streamingText.postValue((_streamingText.value ?: "") + chunk)
                    },
                    onComplete = { fullReply ->
                        Log.d(TAG, "接收完成: $fullReply")
                        _streamingText.postValue("")
                        _isSending.postValue(false)
                    },
                    onError = { error ->
                        Log.e(TAG, "发送失败", error)
                        _errorMessage.postValue("发送失败: ${error.message}")
                        _streamingText.postValue("")
                        _isSending.postValue(false)
                    }
                )
            } catch (e: Exception) {
                Log.e(TAG, "发送异常", e)
                _errorMessage.value = "发送异常: ${e.message}"
                _isSending.value = false
            }
        }
    }

    /**
     * 发送消息（同步模式 - 备用）
     */
    fun sendMessageSync(content: String) {
        if (content.isBlank()) {
            _errorMessage.value = "消息不能为空"
            return
        }

        if (_isSending.value == true) {
            _errorMessage.value = "正在发送中，请稍候..."
            return
        }

        viewModelScope.launch {
            try {
                _isSending.value = true

                val result = repository.sendMessageSync(sessionId, content)

                result.onSuccess { reply ->
                    Log.d(TAG, "发送成功: $reply")
                    _isSending.value = false
                }.onFailure { error ->
                    Log.e(TAG, "发送失败", error)
                    _errorMessage.value = "发送失败: ${error.message}"
                    _isSending.value = false
                }
            } catch (e: Exception) {
                Log.e(TAG, "发送异常", e)
                _errorMessage.value = "发送异常: ${e.message}"
                _isSending.value = false
            }
        }
    }

    /**
     * 清空历史记录
     */
    fun clearHistory() {
        viewModelScope.launch {
            repository.clearHistory(sessionId)
        }
    }

    /**
     * 取消当前SSE连接
     */
    fun cancelCurrentRequest() {
        currentEventSource?.cancel()
        currentEventSource = null
        _isSending.value = false
        _streamingText.value = ""
    }

    override fun onCleared() {
        super.onCleared()
        cancelCurrentRequest()
    }
}

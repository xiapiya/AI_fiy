package com.xiapiya.companion.presentation.common

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import com.xiapiya.companion.data.repository.ChatRepository
import com.xiapiya.companion.presentation.chat.ChatViewModel

/**
 * ViewModel 工厂
 *
 * 用于创建带参数的 ViewModel 实例
 */
class ViewModelFactory(
    private val chatRepository: ChatRepository
) : ViewModelProvider.Factory {

    @Suppress("UNCHECKED_CAST")
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        return when {
            modelClass.isAssignableFrom(ChatViewModel::class.java) -> {
                ChatViewModel(chatRepository) as T
            }
            else -> throw IllegalArgumentException("Unknown ViewModel class: ${modelClass.name}")
        }
    }
}

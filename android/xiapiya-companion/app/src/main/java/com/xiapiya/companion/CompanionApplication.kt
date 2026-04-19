package com.xiapiya.companion

import android.app.Application
import android.util.Log
import com.xiapiya.companion.data.local.AppDatabase
import com.xiapiya.companion.data.remote.RetrofitClient
import com.xiapiya.companion.data.repository.ChatRepository
import com.xiapiya.companion.util.AppConstants

/**
 * Application 类
 *
 * 全局单例，初始化数据库和网络客户端
 */
class CompanionApplication : Application() {

    companion object {
        private const val TAG = "${AppConstants.LOG_TAG}.App"
    }

    // 数据库实例（懒加载）
    val database: AppDatabase by lazy {
        AppDatabase.getDatabase(this)
    }

    // 聊天仓库（懒加载）
    val chatRepository: ChatRepository by lazy {
        ChatRepository(
            chatDao = database.chatDao(),
            apiService = RetrofitClient.chatApiService,
            sseClient = RetrofitClient.sseClient
        )
    }

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "应用启动")
        Log.d(TAG, "Session ID: ${AppConstants.CLIENT_SESSION_ID}")
        Log.d(TAG, "API Base URL: ${AppConstants.API_BASE_URL}")
    }
}

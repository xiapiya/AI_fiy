package com.xiapiya.companion.util

/**
 * 应用全局常量配置
 */
object AppConstants {

    /**
     * 硬编码 Session ID
     *
     * 必须与 ESP32 固件中的 DEVICE_ID 保持一致
     * 云端通过此 ID 索引统一上下文记忆（包括 ESP32 的语音对话历史）
     */
    const val CLIENT_SESSION_ID = "xiapiya_master_01"

    /**
     * 后端 API Base URL
     *
     * 开发环境：使用本机IP或内网IP
     * 生产环境：替换为实际域名
     */
    const val API_BASE_URL = "https://yourdomain.com"

    /**
     * 网络超时配置（毫秒）
     */
    const val CONNECT_TIMEOUT = 15_000L  // 15秒
    const val READ_TIMEOUT = 30_000L     // 30秒
    const val WRITE_TIMEOUT = 30_000L    // 30秒

    /**
     * SSE 配置
     */
    const val SSE_RETRY_INTERVAL = 3_000L  // SSE 断线重连间隔 3秒

    /**
     * 数据库配置
     */
    const val DATABASE_NAME = "xiapiya_chat_db"
    const val DATABASE_VERSION = 1

    /**
     * 日志标签
     */
    const val LOG_TAG = "XiapiyaCompanion"
}

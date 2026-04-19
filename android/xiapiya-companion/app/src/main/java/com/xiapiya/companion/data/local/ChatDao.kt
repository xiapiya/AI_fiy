package com.xiapiya.companion.data.local

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

/**
 * 聊天消息数据访问对象
 */
@Dao
interface ChatDao {

    /**
     * 获取指定 Session 的所有消息（按时间升序）
     *
     * 返回 Flow 实现自动更新 UI
     */
    @Query("SELECT * FROM chat_messages WHERE sessionId = :sessionId ORDER BY timestamp ASC")
    fun getMessagesFlow(sessionId: String): Flow<List<ChatMessage>>

    /**
     * 插入一条新消息
     */
    @Insert
    suspend fun insert(message: ChatMessage)

    /**
     * 插入多条消息（批量）
     */
    @Insert
    suspend fun insertAll(messages: List<ChatMessage>)

    /**
     * 清空指定 Session 的所有消息
     */
    @Query("DELETE FROM chat_messages WHERE sessionId = :sessionId")
    suspend fun clearHistory(sessionId: String)

    /**
     * 获取指定 Session 的消息数量
     */
    @Query("SELECT COUNT(*) FROM chat_messages WHERE sessionId = :sessionId")
    suspend fun getMessageCount(sessionId: String): Int

    /**
     * 删除所有消息
     */
    @Query("DELETE FROM chat_messages")
    suspend fun deleteAll()
}

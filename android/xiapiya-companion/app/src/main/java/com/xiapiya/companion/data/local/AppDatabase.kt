package com.xiapiya.companion.data.local

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import com.xiapiya.companion.util.AppConstants

/**
 * Room 数据库实例
 *
 * 单例模式确保全局只有一个数据库实例
 */
@Database(
    entities = [ChatMessage::class],
    version = AppConstants.DATABASE_VERSION,
    exportSchema = false
)
abstract class AppDatabase : RoomDatabase() {

    /**
     * 获取聊天消息 DAO
     */
    abstract fun chatDao(): ChatDao

    companion object {
        @Volatile
        private var INSTANCE: AppDatabase? = null

        /**
         * 获取数据库实例（单例）
         */
        fun getDatabase(context: Context): AppDatabase {
            return INSTANCE ?: synchronized(this) {
                val instance = Room.databaseBuilder(
                    context.applicationContext,
                    AppDatabase::class.java,
                    AppConstants.DATABASE_NAME
                )
                    .fallbackToDestructiveMigration() // 开发阶段简化迁移
                    .build()
                INSTANCE = instance
                instance
            }
        }

        /**
         * 测试用：销毁数据库实例
         */
        fun destroyInstance() {
            INSTANCE = null
        }
    }
}

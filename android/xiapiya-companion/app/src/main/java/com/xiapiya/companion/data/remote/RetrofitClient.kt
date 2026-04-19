package com.xiapiya.companion.data.remote

import com.google.gson.Gson
import com.google.gson.GsonBuilder
import com.xiapiya.companion.util.AppConstants
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import java.util.concurrent.TimeUnit

/**
 * Retrofit 客户端单例
 */
object RetrofitClient {

    private val gson: Gson by lazy {
        GsonBuilder()
            .setLenient()
            .create()
    }

    private val loggingInterceptor by lazy {
        HttpLoggingInterceptor().apply {
            level = HttpLoggingInterceptor.Level.BODY
        }
    }

    val okHttpClient: OkHttpClient by lazy {
        OkHttpClient.Builder()
            .connectTimeout(AppConstants.CONNECT_TIMEOUT, TimeUnit.MILLISECONDS)
            .readTimeout(AppConstants.READ_TIMEOUT, TimeUnit.MILLISECONDS)
            .writeTimeout(AppConstants.WRITE_TIMEOUT, TimeUnit.MILLISECONDS)
            .addInterceptor(loggingInterceptor)
            .build()
    }

    private val retrofit: Retrofit by lazy {
        Retrofit.Builder()
            .baseUrl(AppConstants.API_BASE_URL)
            .client(okHttpClient)
            .addConverterFactory(GsonConverterFactory.create(gson))
            .build()
    }

    val chatApiService: ChatApiService by lazy {
        retrofit.create(ChatApiService::class.java)
    }

    val sseClient: SseClient by lazy {
        SseClient(okHttpClient, AppConstants.API_BASE_URL, gson)
    }
}

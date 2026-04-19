package com.xiapiya.companion.presentation.chat

import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.inputmethod.EditorInfo
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.button.MaterialButton
import com.google.android.material.textfield.TextInputEditText
import com.xiapiya.companion.CompanionApplication
import com.xiapiya.companion.R
import com.xiapiya.companion.presentation.common.ViewModelFactory

/**
 * 聊天界面 Activity
 */
class ChatActivity : AppCompatActivity() {

    private lateinit var toolbar: MaterialToolbar
    private lateinit var recyclerView: RecyclerView
    private lateinit var inputEditText: TextInputEditText
    private lateinit var sendButton: MaterialButton
    private lateinit var streamingTextLayout: View
    private lateinit var streamingText: android.widget.TextView

    private val chatAdapter = ChatAdapter()

    private val viewModel: ChatViewModel by viewModels {
        val app = application as CompanionApplication
        ViewModelFactory(app.chatRepository)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_chat)

        initViews()
        setupRecyclerView()
        setupInputListeners()
        observeViewModel()
    }

    private fun initViews() {
        toolbar = findViewById(R.id.toolbar)
        recyclerView = findViewById(R.id.recyclerView)
        inputEditText = findViewById(R.id.inputEditText)
        sendButton = findViewById(R.id.sendButton)
        streamingTextLayout = findViewById(R.id.streamingTextLayout)
        streamingText = findViewById(R.id.streamingText)

        setSupportActionBar(toolbar)
    }

    private fun setupRecyclerView() {
        recyclerView.apply {
            layoutManager = LinearLayoutManager(this@ChatActivity)
            adapter = chatAdapter
        }
    }

    private fun setupInputListeners() {
        // 发送按钮点击
        sendButton.setOnClickListener {
            sendMessage()
        }

        // 输入框回车发送
        inputEditText.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_SEND) {
                sendMessage()
                true
            } else {
                false
            }
        }
    }

    private fun observeViewModel() {
        // 观察消息列表
        viewModel.messages.observe(this) { messages ->
            chatAdapter.submitList(messages) {
                // 滚动到最新消息
                if (messages.isNotEmpty()) {
                    recyclerView.smoothScrollToPosition(messages.size - 1)
                }
            }
        }

        // 观察流式渲染文本
        viewModel.streamingText.observe(this) { text ->
            if (text.isNullOrEmpty()) {
                streamingTextLayout.visibility = View.GONE
            } else {
                streamingTextLayout.visibility = View.VISIBLE
                streamingText.text = text
                // 滚动到底部
                recyclerView.post {
                    recyclerView.smoothScrollToPosition(chatAdapter.itemCount)
                }
            }
        }

        // 观察发送状态
        viewModel.isSending.observe(this) { isSending ->
            sendButton.isEnabled = !isSending
            sendButton.text = if (isSending) getString(R.string.sending) else getString(R.string.send_button)
        }

        // 观察错误消息
        viewModel.errorMessage.observe(this) { error ->
            if (!error.isNullOrEmpty()) {
                Toast.makeText(this, error, Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun sendMessage() {
        val content = inputEditText.text?.toString()?.trim() ?: ""
        if (content.isEmpty()) {
            Toast.makeText(this, R.string.error_empty_message, Toast.LENGTH_SHORT).show()
            return
        }

        // 发送消息（SSE 流式模式）
        viewModel.sendMessage(content)

        // 清空输入框
        inputEditText.text?.clear()
    }

    override fun onCreateOptionsMenu(menu: Menu?): Boolean {
        menuInflater.inflate(R.menu.menu_chat, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_clear_history -> {
                showClearHistoryDialog()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun showClearHistoryDialog() {
        AlertDialog.Builder(this)
            .setTitle(R.string.confirm_clear_title)
            .setMessage(R.string.confirm_clear_message)
            .setPositiveButton(R.string.confirm) { _, _ ->
                viewModel.clearHistory()
                Toast.makeText(this, "历史记录已清空", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    override fun onDestroy() {
        super.onDestroy()
        // 取消当前请求
        viewModel.cancelCurrentRequest()
    }
}

#include <WiFi.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <AudioFileSourceSPIFFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <driver/i2s.h>
#include <base64.h>

// WiFi配置 - 请替换为实际WiFi凭证
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

// MQTT配置 - 本地EMQX部署
const char *mqtt_broker = "localhost";  // 或实际EMQX IP地址
const char *mqtt_upload_topic = "emqx/esp32/audio";
const char *mqtt_download_topic = "emqx/esp32/playaudio";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

// I2S录音配置
#define I2S_SAMPLE_RATE   8000
#define I2S_SAMPLE_BITS   16
#define I2S_CHANNEL_NUM   1
#define I2S_REC_PORT      I2S_NUM_0

#define I2S_REC_BCLK  7
#define I2S_REC_LRCL  8
#define I2S_REC_DOUT  9

// I2S播放配置
#define I2S_PLAY_PORT     I2S_NUM_1
#define I2S_PLAY_BCLK  2
#define I2S_PLAY_LRCL  1
#define I2S_PLAY_DOUT  42

#define RECORD_SECONDS    3
#define AUDIO_BUFFER_SIZE (I2S_SAMPLE_RATE * RECORD_SECONDS * 2)

// 语音检测参数
#define ENERGY_THRESHOLD  350      // 语音检测阈值
#define DETECTION_WINDOW  2        // 需要连续检测的次数
#define SILENCE_RESET     5        // 静音样本后重置计数

// LED状态指示配置
#define LED_PIN 2  // 板载LED引脚，根据实际板子调整

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// 音频播放对象
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSPIFFS *file = nullptr;
AudioOutputI2S *out = nullptr;
const char* tempMp3Path = "/temp.mp3";

// 系统状态 - 增加STATE_UPLOADING状态
enum SystemState {
  STATE_IDLE,       // 待机状态
  STATE_RECORDING,  // 录音状态
  STATE_UPLOADING,  // 上传状态
  STATE_PLAYING,    // 播放状态
  STATE_COOLDOWN    // 冷却期状态
};

volatile SystemState currentState = STATE_IDLE;
volatile bool playbackRequested = false;
volatile bool stopRecording = false;

bool i2sRecordInitialized = false;
bool i2sPlayInitialized = false;

// 冷却期和防循环机制
unsigned long cooldownStartTime = 0;
const unsigned long COOLDOWN_DURATION = 3000;  // 3秒冷却期
const unsigned long POST_COOLDOWN_DELAY = 1000;
unsigned long postCooldownTime = 0;

unsigned long lastRecordingTime = 0;
const unsigned long MIN_RECORDING_INTERVAL = 5000;
int consecutiveDetections = 0;
const int MAX_CONSECUTIVE_DETECTIONS = 3;

// 待播放的音频数据
uint8_t* pendingAudioData = nullptr;
size_t pendingAudioLength = 0;

// 语音检测状态
int detectionCount = 0;
int silenceCount = 0;

// LED控制变量
unsigned long lastLedToggle = 0;
bool ledState = false;

/**
 * LED控制函数 - 根据系统状态控制LED行为
 * STATE_IDLE: LED常亮
 * STATE_RECORDING: LED闪烁（500ms间隔）
 * STATE_UPLOADING: LED快闪（200ms间隔）
 * STATE_PLAYING: LED常亮
 * STATE_COOLDOWN: LED关闭
 */
void updateLED() {
  unsigned long currentTime = millis();

  switch (currentState) {
    case STATE_IDLE:
    case STATE_PLAYING:
      // 常亮
      digitalWrite(LED_PIN, HIGH);
      ledState = true;
      break;

    case STATE_RECORDING:
      // 闪烁（500ms间隔）
      if (currentTime - lastLedToggle >= 500) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        lastLedToggle = currentTime;
      }
      break;

    case STATE_UPLOADING:
      // 快闪（200ms间隔）
      if (currentTime - lastLedToggle >= 200) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        lastLedToggle = currentTime;
      }
      break;

    case STATE_COOLDOWN:
      // 关闭
      digitalWrite(LED_PIN, LOW);
      ledState = false;
      break;
  }
}

/**
 * 连接到WiFi网络
 */
void connectToWiFi() {
  Serial.print("正在连接WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("已连接！");
  Serial.printf("IP地址: %s, 信号强度: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

/**
 * 清理I2S录音资源
 */
void cleanupRecording() {
  if (i2sRecordInitialized) {
    i2s_driver_uninstall(I2S_REC_PORT);
    i2sRecordInitialized = false;
    Serial.println("录音I2S已清理");
  }
}

/**
 * 清理音频播放资源
 */
void cleanupPlayback() {
  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  if (file) {
    delete file;
    file = nullptr;
  }
  if (out) {
    out->stop();
    delete out;
    out = nullptr;
  }

  if (i2sPlayInitialized) {
    i2s_driver_uninstall(I2S_PLAY_PORT);
    i2sPlayInitialized = false;
    Serial.println("播放I2S已清理");
  }
}

/**
 * MQTT消息回调处理函数
 * @param topic MQTT主题
 * @param payload 消息负载
 * @param length 负载长度
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("收到MQTT消息: %s, %d 字节\n", topic, length);

  if (strcmp(topic, mqtt_download_topic) != 0) return;

  // 释放之前的待播放音频数据
  if (pendingAudioData) {
    free(pendingAudioData);
  }

  // 为新音频数据分配内存
  pendingAudioData = (uint8_t*)malloc(length);
  if (pendingAudioData) {
    memcpy(pendingAudioData, payload, length);
    pendingAudioLength = length;
    playbackRequested = true;

    // 如果正在录音，则中断录音
    if (currentState == STATE_RECORDING) {
      stopRecording = true;
    }
  } else {
    Serial.println("为音频数据分配内存失败");
  }
}

/**
 * 连接到MQTT Broker
 */
void connectToMQTT() {
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);

  while (!mqtt_client.connected()) {
    Serial.print("正在连接MQTT...");
    String clientId = "esp32-audio-client-" + WiFi.macAddress();

    if (mqtt_client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("已连接到MQTT Broker");
      mqtt_client.subscribe(mqtt_download_topic);
      Serial.printf("已订阅主题: %s\n", mqtt_download_topic);
    } else {
      Serial.printf("连接失败, rc=%d. 5秒后重试。\n", mqtt_client.state());
      delay(5000);
    }
  }
}

/**
 * 初始化I2S录音
 * @return 成功返回true，失败返回false
 */
bool initRecording() {
  if (i2sRecordInitialized) return true;

  cleanupPlayback();
  delay(50);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_REC_BCLK,
    .ws_io_num = I2S_REC_LRCL,
    .data_out_num = -1,
    .data_in_num = I2S_REC_DOUT
  };

  esp_err_t err = i2s_driver_install(I2S_REC_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("录音I2S驱动安装失败: %d\n", err);
    return false;
  }

  err = i2s_set_pin(I2S_REC_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("录音I2S引脚设置失败: %d\n", err);
    i2s_driver_uninstall(I2S_REC_PORT);
    return false;
  }

  i2s_zero_dma_buffer(I2S_REC_PORT);
  i2sRecordInitialized = true;
  Serial.println("录音I2S已初始化");
  return true;
}

/**
 * 初始化I2S播放
 * @return 成功返回true，失败返回false
 */
bool initPlayback() {
  // 先完全清理
  cleanupPlayback();
  cleanupRecording();
  delay(200);  // 等待资源完全释放

  i2s_config_t play_i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t play_pin_config = {
    .bck_io_num = I2S_PLAY_BCLK,
    .ws_io_num = I2S_PLAY_LRCL,
    .data_out_num = I2S_PLAY_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  esp_err_t err = i2s_driver_install(I2S_PLAY_PORT, &play_i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("播放I2S驱动安装失败: %d\n", err);
    return false;
  }

  err = i2s_set_pin(I2S_PLAY_PORT, &play_pin_config);
  if (err != ESP_OK) {
    Serial.printf("播放I2S引脚设置失败: %d\n", err);
    i2s_driver_uninstall(I2S_PLAY_PORT);
    return false;
  }

  i2sPlayInitialized = true;
  Serial.println("播放I2S已初始化");
  return true;
}

/**
 * 计算音频能量级别用于语音检测
 * @param samples 音频样本缓冲区
 * @param count 样本数量
 * @return 音频能量级别
 */
uint32_t calculateAudioEnergy(int16_t *samples, size_t count) {
  uint64_t sum = 0;
  for (size_t i = 0; i < count; i++) {
    int32_t s = abs(samples[i]);  // 使用绝对值
    sum += s;
  }
  return (uint32_t)(sum / count);  // 绝对值的简单平均
}

/**
 * 执行音频录制并通过MQTT发送Base64编码的音频数据
 * 修改点：上传Base64编码的原始PCM数据（不带WAV头）
 */
void performRecording() {
  currentState = STATE_RECORDING;
  stopRecording = false;

  size_t bytes_to_record = AUDIO_BUFFER_SIZE;
  int16_t *audio_buffer = (int16_t *)malloc(bytes_to_record);
  if (!audio_buffer) {
    Serial.println("分配音频缓冲区失败");
    currentState = STATE_IDLE;
    return;
  }

  Serial.println("开始录音...");
  size_t total_bytes_read = 0;
  unsigned long recordStartTime = millis();

  // 录制音频数据
  while (total_bytes_read < bytes_to_record &&
         !stopRecording &&
         (millis() - recordStartTime) < (RECORD_SECONDS * 1000 + 500)) {

    size_t bytes_read = 0;
    size_t bytes_to_read = (total_bytes_read < 1024) ? 1024 : 512;
    if (bytes_to_read > (bytes_to_record - total_bytes_read)) {
      bytes_to_read = bytes_to_record - total_bytes_read;
    }

    esp_err_t result = i2s_read(I2S_REC_PORT,
                                (uint8_t *)audio_buffer + total_bytes_read,
                                bytes_to_read, &bytes_read, 100);

    if (result == ESP_OK && bytes_read > 0) {
      total_bytes_read += bytes_read;
    }

    mqtt_client.loop();
  }

  if (stopRecording) {
    Serial.println("录音被播放请求中断");
    free(audio_buffer);
    currentState = STATE_IDLE;
    return;
  }

  Serial.printf("录音完成，捕获了 %d 字节\n", total_bytes_read);

  // 进入上传状态
  currentState = STATE_UPLOADING;

  // 使用Base64编码原始PCM数据（不带WAV头）
  if (total_bytes_read > 0) {
    // 计算Base64编码后的大小（向上取整到4的倍数）
    size_t base64_size = ((total_bytes_read + 2) / 3) * 4 + 1;  // +1为null终止符
    char *base64_buffer = (char *)malloc(base64_size);

    if (base64_buffer) {
      // 进行Base64编码
      base64_encode(base64_buffer, (char *)audio_buffer, total_bytes_read);

      Serial.printf("Base64编码后大小: %d 字节\n", strlen(base64_buffer));

      // 设置MQTT缓冲区大小
      if (strlen(base64_buffer) > mqtt_client.getBufferSize()) {
        mqtt_client.setBufferSize(strlen(base64_buffer) + 1024);
      }

      // 发送Base64编码的音频数据
      if (mqtt_client.connected()) {
        bool result = mqtt_client.publish(mqtt_upload_topic, base64_buffer);
        Serial.printf("音频上传 (Base64): %s\n", result ? "成功" : "失败");
      }

      free(base64_buffer);
    } else {
      Serial.println("分配Base64缓冲区失败");
    }
  }

  free(audio_buffer);

  // 上传完成后进入冷却期
  currentState = STATE_COOLDOWN;
  cooldownStartTime = millis();
  Serial.println("进入冷却期 (3秒)...");
}

/**
 * 语音检测和录音触发
 */
void detectVoiceAndRecord() {
  // 检查最小录音间隔
  if (millis() - lastRecordingTime < MIN_RECORDING_INTERVAL) {
    return;
  }

  // 检查连续检测限制
  if (consecutiveDetections >= MAX_CONSECUTIVE_DETECTIONS) {
    Serial.println("连续录音次数过多，短暂暂停");
    delay(2000);
    consecutiveDetections = 0;
    return;
  }

  if (!i2sRecordInitialized) {
    if (!initRecording()) {
      delay(1000);
      return;
    }
    // 清除初始缓冲区
    int16_t dummy_buf[512];
    size_t dummy_read;
    for (int i = 0; i < 3; i++) {
      i2s_read(I2S_REC_PORT, dummy_buf, sizeof(dummy_buf), &dummy_read, 50);
      delay(10);
    }
    // 重置检测状态
    detectionCount = 0;
    silenceCount = 0;
  }

  // 检查语音触发
  int16_t sample_buf[256];
  size_t bytes_read = 0;

  esp_err_t result = i2s_read(I2S_REC_PORT, sample_buf, sizeof(sample_buf), &bytes_read, 50);
  if (result != ESP_OK) {
    Serial.println("I2S读取失败，正在重新初始化...");
    cleanupRecording();
    delay(100);
    return;
  }

  if (bytes_read > 0) {
    size_t sample_count = bytes_read / 2;
    uint32_t energy = calculateAudioEnergy(sample_buf, sample_count);

    // 每10个周期输出一次调试信息
    static int debugCounter = 0;
    debugCounter++;
    if (debugCounter >= 10) {
      Serial.printf("能量: %u, 阈值: %d, 计数: %d/%d\n",
                   energy, ENERGY_THRESHOLD, detectionCount, DETECTION_WINDOW);
      debugCounter = 0;
    }

    // 语音检测逻辑
    if (energy > ENERGY_THRESHOLD) {
      detectionCount++;
      silenceCount = 0;

      if (detectionCount >= DETECTION_WINDOW) {
        Serial.printf("检测到语音！能量: %u\n", energy);

        // 重置检测状态
        detectionCount = 0;
        silenceCount = 0;

        // 更新时间和计数器
        lastRecordingTime = millis();
        consecutiveDetections++;

        performRecording();
      }
    } else {
      silenceCount++;
      if (silenceCount >= SILENCE_RESET) {
        detectionCount = 0;  // 静音后重置检测计数
      }
    }
  }
}

/**
 * 播放接收到的MP3音频
 */
void playAudio() {
  if (!pendingAudioData || pendingAudioLength == 0) {
    currentState = STATE_IDLE;
    return;
  }

  currentState = STATE_PLAYING;
  Serial.println("开始播放音频...");

  // 将MP3文件保存到SPIFFS
  if (SPIFFS.exists(tempMp3Path)) {
    SPIFFS.remove(tempMp3Path);
  }

  File f = SPIFFS.open(tempMp3Path, FILE_WRITE);
  if (!f) {
    Serial.println("打开SPIFFS文件失败");
    free(pendingAudioData);
    pendingAudioData = nullptr;
    pendingAudioLength = 0;
    currentState = STATE_IDLE;
    return;
  }

  f.write(pendingAudioData, pendingAudioLength);
  f.close();

  // 释放音频数据内存
  free(pendingAudioData);
  pendingAudioData = nullptr;
  pendingAudioLength = 0;

  // 初始化播放
  if (!initPlayback()) {
    currentState = STATE_IDLE;
    return;
  }

  out = new AudioOutputI2S(I2S_PLAY_PORT);
  out->begin();

  file = new AudioFileSourceSPIFFS(tempMp3Path);
  mp3 = new AudioGeneratorMP3();

  if (!mp3->begin(file, out)) {
    Serial.println("启动MP3播放失败");
    cleanupPlayback();
    currentState = STATE_IDLE;
    return;
  }

  Serial.println("正在播放音频...");

  // 播放循环
  while (mp3 && mp3->isRunning() && !playbackRequested) {
    if (!mp3->loop()) {
      break;
    }
    mqtt_client.loop();
    yield();
  }

  if (playbackRequested) {
    Serial.println("播放被新音频中断");
  } else {
    Serial.println("播放正常完成");
  }

  cleanupPlayback();

  // 播放完成后进入冷却期
  currentState = STATE_COOLDOWN;
  cooldownStartTime = millis();
  Serial.println("进入冷却期 (3秒)...");

  // 播放后重置检测状态
  detectionCount = 0;
  silenceCount = 0;
  consecutiveDetections = 0;
}

/**
 * Arduino setup函数
 */
void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("ESP32音频录制与播放系统启动中...");

  // 初始化LED引脚
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // 初始状态：常亮

  // 初始化SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS挂载失败");
    while (1) delay(1000);
  }
  Serial.println("SPIFFS已挂载");

  // 连接WiFi
  connectToWiFi();

  // 设置MQTT
  mqtt_client.setBufferSize(262144);  // 256KB缓冲区
  mqtt_client.setKeepAlive(60);
  mqtt_client.setSocketTimeout(30);

  connectToMQTT();

  currentState = STATE_IDLE;
  Serial.println("系统就绪！");
  Serial.printf("剩余堆内存: %d 字节\n", ESP.getFreeHeap());
}

/**
 * Arduino主循环
 */
void loop() {
  // 维护连接
  if (!mqtt_client.connected()) {
    Serial.println("MQTT重新连接中...");
    connectToMQTT();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi重新连接中...");
    connectToWiFi();
  }

  mqtt_client.loop();

  // 更新LED状态指示
  updateLED();

  // 处理冷却期状态
  if (currentState == STATE_COOLDOWN) {
    if (millis() - cooldownStartTime >= COOLDOWN_DURATION) {
      Serial.println("冷却期结束，返回待机状态");
      currentState = STATE_IDLE;
      // 重新初始化录音以准备下次录音
      if (!i2sRecordInitialized) {
        initRecording();
      }
    }
    return;  // 冷却期间不处理其他操作
  }

  // 处理待播放的音频
  if (playbackRequested) {
    playbackRequested = false;

    // 如果当前正在播放，则停止
    if (currentState == STATE_PLAYING) {
      cleanupPlayback();
    }

    playAudio();
  }

  // 空闲时进行语音检测和录音
  if (currentState == STATE_IDLE && !playbackRequested) {
    detectVoiceAndRecord();
  }

  delay(10);  // 快速响应
}

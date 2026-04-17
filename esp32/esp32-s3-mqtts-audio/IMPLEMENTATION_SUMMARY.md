# ESP32-S3 TFT屏幕和LVGL UI实现总结 (Phase 1)

## 实现完成时间
2024年4月16日

## 已完成的文件

### 1. TFT显示驱动层
- **main/tft_display.h** - TFT显示屏头文件
  - ILI9341驱动配置
  - SPI引脚定义 (SCK=13, MOSI=11, CS=10, DC=12, RST=3, BL=5)
  - 背光控制接口

- **main/tft_display.c** - TFT显示屏实现
  - SPI总线初始化 (40MHz)
  - ILI9341面板驱动
  - 背光PWM控制 (LEDC)
  - 屏幕尺寸: 240x320

### 2. LVGL UI层
- **main/lvgl_ui.h** - LVGL UI头文件
  - 5种情绪表情定义 (neutral/happy/sad/comfort/thinking)
  - UI控制接口 (set_emotion/set_text/set_wifi_status/set_mqtt_status)

- **main/lvgl_ui.c** - LVGL UI实现
  - LVGL v9初始化
  - PSRAM双缓冲分配
  - FreeRTOS Mutex保护
  - UI组件:
    - 顶部状态栏 (WiFi图标 + MQTT图标)
    - 中间情绪表情 (使用颜色编码的圆形)
    - 底部字幕区域 (滚动文本)
  - 两个任务:
    - lvgl_task: LVGL刷新任务 (200Hz)
    - state_monitor_task: 状态监听任务 (监听状态机Event Group)

### 3. LVGL配置
- **main/lv_conf.h** - LVGL配置文件
  - 颜色深度: 16位 (RGB565)
  - 内存分配: ESP-IDF标准库
  - FreeRTOS操作系统适配
  - 字体配置
  - 组件启用配置

- **main/idf_component.yml** - ESP组件依赖管理
  - lvgl/lvgl: ~9.2.0

### 4. 构建配置
- **main/CMakeLists.txt** - 已添加:
  - tft_display.c
  - lvgl_ui.c
  - esp_lcd依赖
  - lvgl依赖

- **sdkconfig.defaults** - 已添加:
  - SPI性能优化 (CONFIG_SPI_MASTER_ISR_IN_IRAM=y)
  - LVGL内存配置 (CONFIG_LV_MEM_SIZE=131072)

### 5. 主程序集成
- **main/main.c** - 已修改:
  - 添加TFT和LVGL头文件引用
  - 在I2S初始化后初始化TFT
  - 初始化LVGL UI
  - WiFi连接后更新WiFi状态图标
  - MQTT连接后更新MQTT状态图标

## 功能特性

### 情绪表情系统
使用颜色编码的圆形表示不同情绪:
- **NEUTRAL**: 白色 (0xFFFFFF)
- **HAPPY**: 金色 (0xFFD700)
- **SAD**: 蓝色 (0x4169E1)
- **COMFORT**: 绿色 (0x32CD32)
- **THINKING**: 灰色 (0x808080)

### 状态同步
- 监听状态机Event Group
- 自动根据状态切换表情和文本:
  - STATE_IDLE → NEUTRAL + "Listening..."
  - STATE_RECORDING → HAPPY + "Recording..."
  - STATE_CLOUD_SYNC → THINKING + "Thinking..."
  - STATE_PLAYING → COMFORT + "Playing..."

### 线程安全
- 所有UI操作使用FreeRTOS Mutex保护
- 独立的LVGL刷新任务 (优先级2)
- 不阻塞音频任务

### 内存优化
- 使用PSRAM存储LVGL缓冲区
- 缓冲区大小: 240 * 320 * 2 = 153,600 bytes

## 引脚配置总结

### TFT屏幕 (SPI)
- SCK:  GPIO 13
- MOSI: GPIO 11
- CS:   GPIO 10
- DC:   GPIO 12
- RST:  GPIO 3
- BL:   GPIO 5

### 已占用引脚 (避免冲突)
- I2S音频: GPIO 1, 2, 4, 7, 8, 9
- 其他保留: GPIO 0, 19, 20

## 待验证项

### 编译测试
- [ ] 运行 `idf.py build` 验证编译通过
- [ ] 检查LVGL组件是否正确下载

### 功能测试
- [ ] TFT屏幕正常显示
- [ ] 5种情绪表情切换正常
- [ ] 字幕文本显示正常
- [ ] 网络状态图标更新正常
- [ ] 帧率 ≥30FPS
- [ ] VAD能正常触发 (不被UI阻塞)

### 性能测试
- [ ] 运行1小时后内存无泄漏
- [ ] 无看门狗超时
- [ ] 音频播放不卡顿

## 下一步工作

### 优化表情显示
当前使用简单的颜色圆形，后续可优化为:
1. 使用PNG图片转换为LVGL图像数组
2. 添加动画效果 (旋转、缩放、淡入淡出)
3. 添加更多表情细节

### 添加中文字体支持
1. 转换TTF字体为LVGL格式
2. 在lv_conf.h中启用中文字符范围
3. 更新字幕显示支持中文

### 添加更多UI元素
1. 电池电量显示
2. 音量调节指示
3. 录音波形可视化
4. 云端响应延迟显示

## 技术参考
- ESP-IDF LCD示例: `examples/peripherals/lcd/spi_lcd_touch`
- LVGL文档: https://docs.lvgl.io/master/
- ILI9341数据手册: https://www.lcdwiki.com/zh/2.2inch_SPI_Module_ILI9341_SKU%3AMSP2202

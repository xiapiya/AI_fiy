# ESP32-S3 MQTTS音频通信项目
# 基于ESP-IDF v5.x

cmake_minimum_required(VERSION 3.16)

# 包含ESP-IDF项目配置
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 设置项目名称
project(esp32_s3_mqtts_audio)

# 嵌入CA证书到固件
target_add_binary_data(${PROJECT_NAME}.elf "certs/ca.pem" TEXT)

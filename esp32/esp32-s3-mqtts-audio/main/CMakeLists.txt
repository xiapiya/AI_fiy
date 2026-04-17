# main组件的CMakeLists.txt

idf_component_register(
    SRCS
        "main.c"
        "i2s_audio.c"
        "mqtt_app.c"
        "vad.c"
        "state_machine.c"
        "tft_display.c"
        "lvgl_ui.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver
        esp_wifi
        nvs_flash
        mqtt
        mbedtls
        esp_http_client
        esp_psram
        esp_driver_i2s
        esp_lcd
        lvgl
)

# 告诉LVGL使用我们的lv_conf.h
target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_CONF_INCLUDE_SIMPLE=1)

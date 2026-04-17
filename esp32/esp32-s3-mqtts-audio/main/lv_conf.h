/**
 * @file lv_conf.h
 * @brief LVGL v9配置文件
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* TFT screen resolution (needed for buffer size calculation) */
#define TFT_H_RES 240
#define TFT_V_RES 320

/*====================
   COLOR SETTINGS
 *====================*/
/* Color depth: 16 (RGB565) */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/*====================
   MEMORY SETTINGS
 *====================*/
/* Use custom memory allocator */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_ESP_IDF
#define LV_USE_STDLIB_STRING    LV_STDLIB_ESP_IDF
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_ESP_IDF

/* Size of the memory available for `lv_malloc()` in bytes */
#define LV_MEM_SIZE    (128 * 1024U)

/*====================
   HAL SETTINGS
 *====================*/
/* Default display buffer size. Buffer size can be set individually for the displays. */
#define LV_DISPLAY_DEF_BUF_SIZE (TFT_H_RES * TFT_V_RES / 10)

/*====================
   OS interface
 *====================*/
/* FreeRTOS */
#define LV_USE_OS   LV_OS_FREERTOS

/*====================
   DRAWING
 *====================*/
#define LV_DRAW_SW_SUPPORT_RGB565    1
#define LV_DRAW_SW_SUPPORT_RGB888    0
#define LV_DRAW_SW_SUPPORT_XRGB8888  0
#define LV_DRAW_SW_SUPPORT_ARGB8888  0

/*====================
   FONT USAGE
 *====================*/
/* Montserrat fonts with various styles and sizes */
#define LV_FONT_MONTSERRAT_8     0
#define LV_FONT_MONTSERRAT_10    0
#define LV_FONT_MONTSERRAT_12    0
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_16    1
#define LV_FONT_MONTSERRAT_18    0
#define LV_FONT_MONTSERRAT_20    1
#define LV_FONT_MONTSERRAT_22    0
#define LV_FONT_MONTSERRAT_24    0
#define LV_FONT_MONTSERRAT_26    0
#define LV_FONT_MONTSERRAT_28    0
#define LV_FONT_MONTSERRAT_30    0
#define LV_FONT_MONTSERRAT_32    0
#define LV_FONT_MONTSERRAT_34    0
#define LV_FONT_MONTSERRAT_36    0
#define LV_FONT_MONTSERRAT_38    0
#define LV_FONT_MONTSERRAT_40    0
#define LV_FONT_MONTSERRAT_42    0
#define LV_FONT_MONTSERRAT_44    0
#define LV_FONT_MONTSERRAT_46    0
#define LV_FONT_MONTSERRAT_48    0

/* Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   WIDGETS
 *====================*/
#define LV_USE_ANIMIMG       1
#define LV_USE_ARC           1
#define LV_USE_BAR           1
#define LV_USE_BTN           1
#define LV_USE_BTNMATRIX     1
#define LV_USE_CANVAS        1
#define LV_USE_CHECKBOX      1
#define LV_USE_DROPDOWN      1
#define LV_USE_IMG           1
#define LV_USE_LABEL         1
#define LV_USE_LINE          1
#define LV_USE_MENU          0
#define LV_USE_MSGBOX        0
#define LV_USE_ROLLER        0
#define LV_USE_SCALE         0
#define LV_USE_SLIDER        1
#define LV_USE_SPAN          0
#define LV_USE_SPINBOX       0
#define LV_USE_SPINNER       1
#define LV_USE_SWITCH        1
#define LV_USE_TEXTAREA      0
#define LV_USE_TABLE         0
#define LV_USE_TABVIEW       0
#define LV_USE_TILEVIEW      0
#define LV_USE_WIN           0

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT 1

/*====================
   LAYOUTS
 *====================*/
#define LV_USE_FLEX    1
#define LV_USE_GRID    1

/*====================
   OTHERS
 *====================*/
#define LV_USE_SNAPSHOT 0
#define LV_USE_MONKEY   0

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG      1
#if LV_USE_LOG
    #define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF   1
#endif

/*====================
   ASSERTS
 *====================*/
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*====================
   TICK
 *====================*/
/* Use FreeRTOS tick */
#define LV_TICK_CUSTOM 0

#endif /* LV_CONF_H */

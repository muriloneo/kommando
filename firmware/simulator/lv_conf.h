/**
 * lv_conf.h — LVGL configuration for the Kommando UI desktop simulator.
 *
 * Copy this file next to the LVGL source or pass -DLV_CONF_PATH="..." to
 * the compiler so LVGL can find it.
 *
 * Based on lv_conf_template.h from LVGL v9.5.
 */

#if 1  /* Activate this file */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ============================================================
   COLOR SETTINGS
   ============================================================ */
#define LV_COLOR_DEPTH          16   /* 16-bit (RGB565) matches the ST7789 panel */

/* ============================================================
   MEMORY
   ============================================================ */
#define LV_MEM_SIZE             (128U * 1024U)   /* 128 KB heap for LVGL */
#define LV_MEM_POOL_INCLUDE     <stdlib.h>
#define LV_MEM_POOL_ALLOC       malloc
#define LV_MEM_POOL_FREE        free

/* ============================================================
   HAL  (tick supplied by the SDL main loop)
   ============================================================ */
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  <SDL2/SDL.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR  ((uint32_t)SDL_GetTicks())

/* ============================================================
   DRAWING
   ============================================================ */
#define LV_DRAW_BUF_ALIGN       4

/* ============================================================
   FONTS
   ============================================================ */
#define LV_FONT_MONTSERRAT_8    0
#define LV_FONT_MONTSERRAT_10   0
#define LV_FONT_MONTSERRAT_12   0
#define LV_FONT_MONTSERRAT_14   1   /* Used for header, tile names, labels */
#define LV_FONT_MONTSERRAT_16   0
#define LV_FONT_MONTSERRAT_18   0
#define LV_FONT_MONTSERRAT_20   0
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   1   /* Used for dimmer value display */
#define LV_FONT_MONTSERRAT_26   0
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/* Symbol font — required for LV_SYMBOL_HOME, LV_SYMBOL_POWER, etc. */
#define LV_FONT_MONTSERRAT_28_COMPRESSED  0

/* ============================================================
   WIDGETS
   ============================================================ */
#define LV_USE_LABEL    1
#define LV_USE_SLIDER   1
#define LV_USE_SPINNER  1
#define LV_USE_ARC      1
#define LV_USE_OBJ_NAME 0

/* ============================================================
   ANIMATION
   (Enable for the upcoming animation features)
   ============================================================ */
#define LV_USE_ANIM     1

/* ============================================================
   SDL2 DISPLAY + INPUT DRIVER
   ============================================================ */
#define LV_USE_SDL      1
#define LV_SDL_INCLUDE_PATH     <SDL2/SDL.h>
/* Use software rendering (no GPU/OpenGL dependency) */
#define LV_SDL_RENDER_MODE      LV_DISPLAY_RENDER_MODE_PARTIAL
#define LV_SDL_BUF_COUNT        2

/* ============================================================
   LOG
   ============================================================ */
#define LV_USE_LOG          1
#define LV_LOG_LEVEL        LV_LOG_LEVEL_INFO
#define LV_LOG_PRINTF       1

/* ============================================================
   OTHER
   ============================================================ */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

#endif /* LV_CONF_H */
#endif /* End of "Content enable" */

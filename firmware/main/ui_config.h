/*
 * Copyright (C) 2026 Murilo Freire
 *
 * This file is part of Kommando firmware.
 *
 * Kommando firmware is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Kommando firmware is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ui_config.h — Platform-independent UI configuration for Kommando Panel.
 *
 * Safe to include in firmware (ESP32) and desktop simulator alike.
 * No ESP32 SDK headers, no RTOS headers required.
 */
#pragma once

/* ============================================================
   DISPLAY GEOMETRY
   ============================================================ */
#define LCD_H_RES              172   /* Horizontal resolution (pixels) */
#define LCD_V_RES              320   /* Vertical resolution (pixels)   */

/* ============================================================
   UI LAYOUT
   ============================================================ */
#define MAX_TILES              6     /* 2 columns × 3 rows             */
#define TILE_NAME_MAX_LEN      32    /* Max chars in friendly name     */
#define TILE_GRID_COLS         2
#define TILE_GRID_ROWS         3

#define HEADER_HEIGHT_PX       28    /* Top bar height (px)            */
#define TILE_CORNER_RADIUS     12    /* Rounded corner radius (px)     */
#define TILE_ICON_OFFSET_Y   (-10)   /* Icon Y offset from tile centre */
#define TILE_NAME_OFFSET_Y     14    /* Name Y offset from tile centre */

/* Dimmer overlay widget sizes */
#define UI_DIMMER_CARD_W      (LCD_H_RES - 28)   /* Card width (px)     */
#define UI_DIMMER_CARD_H       180                /* Card height (px)    */
#define UI_DIMMER_CARD_RADIUS  12                 /* Card corner radius  */
#define UI_SLIDER_W            26                 /* Slider width (px)   */
#define UI_SLIDER_H            110                /* Slider height (px)  */
#define UI_BADGE_SIZE          10                 /* Dimmer badge size   */
#define UI_BADGE_RADIUS        6                  /* Dimmer badge radius */
#define UI_CONN_SPINNER_SIZE   44                 /* Connecting spinner  */

/* ============================================================
   TIMING
   ============================================================ */
#define TILE_LONG_PRESS_MS       300    /* Long-press threshold → open dimmer  */
#define DIMMER_IDLE_TIMEOUT_MS   3500   /* Auto-close dimmer after idle (ms)   */

/* ============================================================
   COLORS  (R, G, B byte triplets for lv_color_make)
   ============================================================ */

/* Screen / header */
#define COLOR_BG               0x0B, 0x12, 0x1F   /* Deep navy background          */
#define COLOR_BG_SCREEN        0x0B, 0x12, 0x1F   /* Alias                         */
#define COLOR_HEADER_BG        0x05, 0x0B, 0x15   /* Darker header bar             */
#define COLOR_BG_HEADER        0x05, 0x0B, 0x15   /* Alias                         */
#define COLOR_HEADER_TEXT      0x90, 0xA0, 0xB0   /* Muted blue-grey text          */

/* Tile states */
#define COLOR_TILE_UNCONFIGURED 0x1B, 0x25, 0x38  /* Very dark (unconfigured)      */
#define COLOR_TILE_EMPTY        0x1B, 0x25, 0x38  /* Alias                         */
#define COLOR_TILE_OFF          0x22, 0x2C, 0x42  /* Dark blue tile (OFF)          */
#define COLOR_TILE_ON           0xFF, 0xCC, 0x00  /* Vivid yellow (ON)             */
#define COLOR_TILE_ON_NIGHT     0x80, 0x66, 0x00  /* Dimmed gold in night mode     */
#define COLOR_ICON_UNCONFIGURED 0x80, 0x90, 0xA0  /* Grey icon when unconfigured   */
#define COLOR_TILE_UNCFG_TEXT   0x80, 0x90, 0xA0  /* Alias                         */
#define COLOR_STATE_TEXT        0xA0, 0xA0, 0xA0  /* Grey state label              */
#define COLOR_STATE_LABEL       0xA0, 0xA0, 0xA0  /* Alias                         */

/* Overlay / dimmer panel */
#define COLOR_DIMMER_CARD_BG    0x2B, 0x2B, 0x2B  /* Dimmer overlay card bg        */
#define COLOR_SLIDER_TRACK      0x3A, 0x3A, 0x3A  /* Slider track background       */
/* Slider fill intentionally matches COLOR_TILE_ON */
#define COLOR_BADGE_DIMMABLE    0xFF, 0xA5, 0x00  /* Orange dimmable indicator dot */

/* ============================================================
   OVERLAY OPACITIES  (raw byte equivalents of LV_OPA_* macros)
   ============================================================ */
#define UI_OVERLAY_OPA_DIM      153   /* LV_OPA_60 — dimmer backdrop   */
#define UI_OVERLAY_OPA_CONN     178   /* LV_OPA_70 — connection overlay */

/* ============================================================
   PLATFORM-AGNOSTIC LOGGING
   On ESP32 the macros forward to ESP-IDF; on desktop they use printf.
   ============================================================ */
#ifdef ESP_PLATFORM
#  include "esp_log.h"
#  define UI_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#  define UI_LOGD(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#  define UI_LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#else
#  include <stdio.h>
#  define UI_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#  define UI_LOGD(tag, fmt, ...) /* suppressed in sim */
#  define UI_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

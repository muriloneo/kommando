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
 * config.h — Kommando Panel: Central Configuration
 */

#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"

/* Fallbacks for environments where Kconfig symbols are not generated yet. */
#ifndef CONFIG_KOMMANDO_OTA_IMAGE_TYPE
#define CONFIG_KOMMANDO_OTA_IMAGE_TYPE 0x1011
#endif

#ifndef CONFIG_KOMMANDO_OTA_HW_VERSION
#define CONFIG_KOMMANDO_OTA_HW_VERSION 0x0101
#endif

#ifndef CONFIG_KOMMANDO_OTA_QUERY_INTERVAL_MIN
#define CONFIG_KOMMANDO_OTA_QUERY_INTERVAL_MIN 60
#endif

#ifndef CONFIG_KOMMANDO_OTA_MAX_DATA_SIZE
#define CONFIG_KOMMANDO_OTA_MAX_DATA_SIZE 223
#endif

/* ============================================================
   FIRMWARE VERSION (semantic versioning)
   ============================================================ */
#define FW_VERSION_MAJOR      1
#define FW_VERSION_MINOR      0
#define FW_VERSION_PATCH      0
#define FW_VERSION_STRING     "1.0.0"

/* ============================================================
   HARDWARE PINS — LCD (ST7789V2, SPI)
   ──────────────────────────────────────────────────────────────
   FPC Pin   Wire colour   GPIO   Function
   ──────    ────────────  ────   ────────
     3       Orange        7      SPI MOSI
     4       Green         6      SPI CLK
     5       Blue         10      SPI CS
     6       Purple       11      Data / Command (DC)
     7       Gray          3      LCD Reset
     8       Orange        2      Backlight (LEDC PWM)
   ============================================================ */
#define PIN_LCD_MOSI     GPIO_NUM_7   /* FPC 3  (Orange)  — SPI MOSI           */
#define PIN_LCD_CLK      GPIO_NUM_6   /* FPC 4  (Green)   — SPI clock          */
#define PIN_LCD_CS       GPIO_NUM_10  /* FPC 5  (Blue)    — SPI chip select    */
#define PIN_LCD_DC       GPIO_NUM_11  /* FPC 6  (Purple)  — Data/command       */
#define PIN_LCD_RST      GPIO_NUM_3   /* FPC 7  (Gray)    — Panel reset        */
#define PIN_LCD_BL       GPIO_NUM_2   /* FPC 8  (Orange)  — Backlight (PWM)    */

/* ============================================================
   HARDWARE PINS — TOUCH (CST816S, I2C)
   ──────────────────────────────────────────────────────────────
   FPC Pin   Wire colour   GPIO   Function
   ──────    ────────────  ────   ────────
     9       Green         0      I2C SDA
    10       Blue          1      I2C SCL
    12       Gray          4      INT (active low, deep-sleep wake)
    16       —            20      Touch reset
   ============================================================ */
#define PIN_TOUCH_SDA    GPIO_NUM_0   /* FPC 9  (Green)   — I2C data           */
#define PIN_TOUCH_SCL    GPIO_NUM_1   /* FPC 10 (Blue)    — I2C clock          */
#define PIN_TOUCH_INT    GPIO_NUM_4   /* FPC 12 (Gray)    — CST816 INT         */
                                      /* Deep-sleep wake capable (C6: GPIO 0-7) */
#define PIN_TOUCH_RST    GPIO_NUM_20  /* FPC 16           — Touch reset        */

/* ============================================================
   HARDWARE PINS — PERIPHERALS
   ============================================================ */
#define PIN_STATUS_LED   GPIO_NUM_8   /* WS2812 RGB status LED                   */
#define PIN_BUTTON       GPIO_NUM_9   /* Physical button (active low)             */

/* ============================================================
   DISPLAY CONFIGURATION
   ============================================================ */
#define LCD_H_RES              172              /* Horizontal resolution in pixels */
#define LCD_V_RES              320              /* Vertical resolution in pixels   */
#define LCD_DRAW_BUF_LINES     40              /* DMA draw buffer height           */
#define LCD_SPI_HOST_ID        SPI2_HOST       /* SPI peripheral to use            */
#define LCD_SPI_HOST           SPI2_HOST       /* Alias used by display.c           */
#define LCD_SPI_CLK_HZ         (40 * 1000 * 1000) /* SPI clock: 40 MHz           */
#define LCD_SPI_MODE           2               /* ST7789 uses SPI mode 2           */
#define LCD_OFFSET_X           34              /* Panel X offset (partial display) */
#define LCD_OFFSET_Y           0               /* Panel Y offset                   */
#define LCD_INVERT_COLOR       true            /* ST7789V2 requires color inversion */
#define LCD_SWAP_XY            false
#define LCD_MIRROR_X           false
#define LCD_MIRROR_Y           false

/* ============================================================
   TOUCH CONFIGURATION
   ============================================================ */
#define TOUCH_I2C_PORT         I2C_NUM_0
#define TOUCH_I2C_CLK_HZ       400000          /* 400 kHz fast mode                */

/* ============================================================
   BACKLIGHT / LEDC PWM
   ============================================================ */
#define BL_LEDC_CHANNEL        LEDC_CHANNEL_0
#define BL_LEDC_TIMER          LEDC_TIMER_0
#define BL_LEDC_SPEED_MODE     LEDC_LOW_SPEED_MODE
#define BL_PWM_FREQ_HZ         5000            /* PWM frequency                    */
#define BL_PWM_RESOLUTION      LEDC_TIMER_8_BIT /* 0–255 duty cycle                */
#define BL_LEVEL_DEFAULT       255             /* Full brightness on boot          */
#define BL_LEVEL_DIM           10              /* Brightness when screen times out */
#define BL_LEVEL_NIGHT         5              /* Brightness in night mode          */

/* ============================================================
   TIMING & INACTIVITY
   ============================================================ */
#define SCREEN_TIMEOUT_DEFAULT_SEC   30        /* Seconds before screen dims (0=off) */
#define SLEEP_TIMEOUT_DEFAULT_SEC    300       /* Seconds before deep sleep (0=off)  */
#define SLEEP_CHECK_INTERVAL_MS      1000      /* How often lcd_task checks idle time */
#define ZB_STEERING_RETRY_MS         2000      /* Retry delay after steering failure  */
#define ZB_LEAVE_RETRY_MS            3000      /* Retry delay after LEAVE signal      */
#define ZB_REPORT_DELAY_MS           10        /* Delay before sending tile report    */
#define TILE_LONG_PRESS_MS           300       /* Tile long-press opens dimmer        */
#define DIMMER_IDLE_TIMEOUT_MS       3500      /* Auto-close dimmer after idle         */
#define BUTTON_LONG_PRESS_MS         5000      /* Long press = re-pair               */
#define BUTTON_FACTORY_RESET_MS      10000     /* Very long press = factory reset    */

/* ============================================================
   ZIGBEE — DEVICE IDENTITY
   ============================================================ */
#define ZB_MANUFACTURER_NAME   "Kommando"
#define ZB_MODEL_ID            "Kommando_Nano"
#define ZB_TX_POWER            20              /* dBm                              */

/* Zigbee OTA identity (must match OTA image metadata and Z2M index) */
#define ZB_OTA_MANUFACTURER_CODE   0x1234
#define ZB_OTA_IMAGE_TYPE          CONFIG_KOMMANDO_OTA_IMAGE_TYPE
#define ZB_OTA_HW_VERSION          CONFIG_KOMMANDO_OTA_HW_VERSION
#define ZB_OTA_QUERY_INTERVAL_MIN  CONFIG_KOMMANDO_OTA_QUERY_INTERVAL_MIN
#define ZB_OTA_MAX_DATA_SIZE       CONFIG_KOMMANDO_OTA_MAX_DATA_SIZE

/* OTA file version encoding: app_release.app_build.stack_release.stack_build */
#define ZB_OTA_FILE_VERSION \
   (((uint32_t)(FW_VERSION_MAJOR) << 24) | \
    ((uint32_t)(FW_VERSION_MINOR) << 16) | \
    ((uint32_t)(FW_VERSION_PATCH) << 8)  | \
    ((uint32_t)(0x00)))
#define ZB_OTA_DOWNLOADED_FILE_VERSION 0xFFFFFFFF

/* ============================================================
   ZIGBEE — ENDPOINTS & CLUSTERS
   Endpoint 1:   UI control + Tile 0 action source (custom cluster + OnOff)
   Endpoints 2–6: Tile 1–5 action sources (OnOff only)
   ============================================================ */
#define ZB_FIRST_ENDPOINT      1               /* Endpoint for tile 0 + control    */
#define ZB_CUSTOM_CLUSTER_ID   0xFC11          /* Manufacturer-specific UI cluster */
#define ZB_MANUF_CODE          0x1234          /* Manufacturer code for custom ZCL */

/* Attribute 0x0000 — write-only: HA → ESP32 (C: config, S: state, N: night...) */
#define ZB_ATTR_PAYLOAD_ID     0x0000

/* Attribute 0x0001 — read+report: ESP32 → HA (T:<tile>:<state> action string)  */
#define ZB_ATTR_STATE_ID       0x0001

/* Device-setting attributes — R/W, persisted to NVS, exposed by Z2M as controls */
#define ZB_ATTR_BACKLIGHT_ID         0x0010   /* UINT8  — LCD brightness 0–255        */
#define ZB_ATTR_SCREEN_TIMEOUT_ID    0x0011   /* UINT16 — dim timeout in seconds       */
#define ZB_ATTR_SCR_TIMEOUT_ID       0x0011   /* Alias for zigbee.c                    */
#define ZB_ATTR_DIM_LEVEL_ID         0x0012   /* UINT8  — brightness when dimmed       */
#define ZB_ATTR_NIGHT_MODE_ID        0x0013   /* BOOL   — night mode on/off            */
#define ZB_ATTR_NIGHT_BRIGHTNESS_ID  0x0014   /* UINT8  — brightness in night mode     */
#define ZB_ATTR_NIGHT_BL_ID          0x0014   /* Alias for zigbee.c                    */
#define ZB_ATTR_DEEP_SLEEP_EN_ID     0x0015   /* BOOL   — enable deep sleep            */
#define ZB_ATTR_DEEP_SLEEP_ID        0x0015   /* Alias for zigbee.c                    */
#define ZB_ATTR_SLEEP_TIMEOUT_ID     0x0016   /* UINT16 — deep-sleep timeout (seconds) */

/* ============================================================
   UI LAYOUT
   ============================================================ */
#define MAX_TILES              6               /* 2 columns × 3 rows                */
#define TILE_NAME_MAX_LEN      32              /* Max characters in tile name        */
#define HEADER_HEIGHT_PX       28              /* Top bar height                     */
#define TILE_CORNER_RADIUS     12              /* Rounded corner radius, pixels      */
#define TILE_ICON_OFFSET_Y     (-10)           /* Icon vertical offset from center   */
#define TILE_NAME_OFFSET_Y     14             /* Name vertical offset from center   */
#define TILE_GRID_COLS         2
#define TILE_GRID_ROWS         3

/* ============================================================
   UI COLORS  (0xRR, 0xGG, 0xBB byte triplets for lv_color_make)
   ============================================================ */
#define COLOR_BG_SCREEN         0x0B, 0x12, 0x1F   /* Deep navy background          */
#define COLOR_BG                0x0B, 0x12, 0x1F   /* Alias for ui_panel.c          */
#define COLOR_BG_HEADER         0x05, 0x0B, 0x15   /* Darker header bar             */
#define COLOR_HEADER_BG         0x05, 0x0B, 0x15   /* Alias for ui_panel.c          */
#define COLOR_HEADER_TEXT       0x90, 0xA0, 0xB0   /* Muted blue-grey text          */
#define COLOR_TILE_UNCONFIGURED  0x1B, 0x25, 0x38   /* Very dark tile (unconfigured) */
#define COLOR_TILE_EMPTY        0x1B, 0x25, 0x38   /* Alias for ui_panel.c          */
#define COLOR_TILE_OFF          0x22, 0x2C, 0x42   /* Dark blue tile (OFF)          */
#define COLOR_TILE_ON           0xFF, 0xCC, 0x00   /* Vivid yellow (ON)             */
#define COLOR_TILE_ON_NIGHT     0x80, 0x66, 0x00   /* Dimmed gold in night mode     */
#define COLOR_ICON_UNCONFIGURED  0x80, 0x90, 0xA0   /* Grey icon when unconfigured   */
#define COLOR_TILE_UNCFG_TEXT   0x80, 0x90, 0xA0   /* Alias for ui_panel.c          */
#define COLOR_STATE_TEXT        0xA0, 0xA0, 0xA0   /* Grey state label              */
#define COLOR_STATE_LABEL       0xA0, 0xA0, 0xA0   /* Alias for ui_panel.c          */

/* ============================================================
   TASK SETTINGS
   ============================================================ */
#define TASK_ZB_STACK_SIZE           8192
#define TASK_ZB_STACK                8192   /* Alias for main.c                   */
#define TASK_ZB_PRIORITY             5
#define TASK_ZB_PRIO                 5      /* Alias for main.c                   */
#define TASK_TILE_REPORT_STACK_SIZE  4096
#define TASK_REPORT_STACK            4096   /* Alias for main.c                   */
#define TASK_TILE_REPORT_PRIORITY    4
#define TASK_REPORT_PRIO             4      /* Alias for main.c                   */
#define TASK_LCD_STACK_SIZE          8192
#define TASK_LCD_STACK               8192   /* Alias for main.c                   */
#define TASK_LCD_PRIORITY            3
#define TASK_LCD_PRIO                3      /* Alias for main.c                   */

#define TILE_EVENT_QUEUE_SIZE        8     /* Max queued touch events              */
#define ZB_EVT_RING_SIZE             8     /* Ring buffer for ZB alarm callbacks   */

/* ============================================================
   NVS — STORAGE KEYS
   ============================================================ */
#define NVS_NAMESPACE          "ui_panel"
#define NVS_KEY_TILE_FMT       "tile_%d"    /* e.g. "tile_0" .. "tile_5"           */
#define NVS_KEY_BACKLIGHT      "bl_level"
#define NVS_KEY_SCREEN_TIMEOUT "scr_timeout"
#define NVS_KEY_DIM_LEVEL      "dim_level"
#define NVS_KEY_NIGHT_MODE     "night_mode"
#define NVS_KEY_NIGHT_BL       "night_bl"
#define NVS_KEY_DEEP_SLEEP_EN  "dsleep_en"
#define NVS_KEY_SLEEP_TIMEOUT  "sleep_to"
#define NVS_KEY_ZB_RECOVERY_DONE "zb_fix_done"

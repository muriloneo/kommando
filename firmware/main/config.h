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

/*
 * Platform-independent UI constants (dimensions, colors, timing).
 * Including this here makes them available to all ESP32 modules that
 * include config.h, while also letting the simulator include ui_config.h
 * directly without pulling in any ESP32 SDK headers.
 */
#include "ui/ui_config.h"

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
   FPC pad   Signal    ESP32-C6 GPIO   Function
   ───────   ───────   ─────────────   ────────
     5       MOSI      GPIO7           SPI data out (C6 pin role: FSPID)
     4       SCLK      GPIO6           SPI clock    (C6 pin role: FSPICLK)
     9       LCD_CS    GPIO3           Chip select
     7       LCD_DC    GPIO10          Data / Command
     8       LCD_RST   GPIO11          Panel reset
     2       LCD_BL    GPIO2           Backlight (LEDC PWM)
   NOTE: U4 net labels on pads 4/5 are swapped (net "LCD_DIN" sits on the SCLK
   contact, "LCD_CLK" on MOSI). The copper decides, not the label.
   ============================================================ */
#define PIN_LCD_MOSI     GPIO_NUM_7   /* U4/FPC pad 5 — SPI data out (FSPID)   */
#define PIN_LCD_CLK      GPIO_NUM_6   /* U4/FPC pad 4 — SPI clock    (FSPICLK) */
#define PIN_LCD_CS       GPIO_NUM_3   /* U4/FPC pad 9 — SPI chip select        */
#define PIN_LCD_DC       GPIO_NUM_10  /* U4/FPC pad 7 — Data/command           */
#define PIN_LCD_RST      GPIO_NUM_11  /* U4/FPC pad 8 — Panel reset            */
#define PIN_LCD_BL       GPIO_NUM_2   /* U4/FPC pad 2 — Backlight (PWM)        */

/* ============================================================
   HARDWARE PINS — TOUCH (CST816S, I2C)
   ──────────────────────────────────────────────────────────────
   FPC pad   Signal    ESP32-C6 GPIO   Function
   ───────   ───────   ─────────────   ────────
    14       TP_SDA    GPIO4           I2C data
    13       TP_SCL    GPIO5           I2C clock
    15       TP_INT    GPIO20          INT (active low; NOT a wake source - C6 LP_IO wake is GPIO0-7)
    16       TP_RST    GPIO19          Touch reset
   NOTE: U4 net labels on pads 13/14 are swapped vs the module's own pinout
   ("TP_SDA" sits on the TP_SCL contact).
   ============================================================ */
#define PIN_TOUCH_SDA    GPIO_NUM_4   /* U4/FPC pad 14 — I2C data              */
#define PIN_TOUCH_SCL    GPIO_NUM_5   /* U4/FPC pad 13 — I2C clock             */
#define PIN_TOUCH_INT    GPIO_NUM_20  /* U4/FPC pad 15 — CST816 INT            */
                                      /* NOT a wake source: C6 LP_IO wake is GPIO0-7 only */
#define PIN_TOUCH_RST    GPIO_NUM_19  /* U4/FPC pad 16 — Touch reset           */

/* ============================================================
   HARDWARE PINS — PERIPHERALS
   ============================================================ */
#define PIN_STATUS_LED   GPIO_NUM_15  /* WS2812 RGB status LED                   */
#define PIN_BUTTON       GPIO_NUM_0   /* Physical button (active low)             */

/* ============================================================
   DISPLAY CONFIGURATION  (LCD_H_RES / LCD_V_RES live in ui_config.h)
   ============================================================ */
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
/* TILE_LONG_PRESS_MS and DIMMER_IDLE_TIMEOUT_MS are in ui_config.h */
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

/* UI_LAYOUT, UI_COLORS, TILE_LONG_PRESS_MS, DIMMER_IDLE_TIMEOUT_MS are now
   in ui_config.h (included at the top of this file). */

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

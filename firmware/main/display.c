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
 */

#include "display.h"
#include "backlight.h"
#include "tiles.h"    /* s_last_touch_tick                               */
#include "config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_sleep.h"

#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "display";

volatile bool g_lvgl_ready = false;

/* Shared flag: true when screen is ACTIVE (not dimmed/sleeping).
 * Written by display_timeout_task(), read by tile_event_cb(). */
volatile bool g_screen_active = true;

/* Retained handles so the timeout task can put the LCD into HW sleep */
static esp_lcd_panel_io_handle_t s_lcd_io   = NULL;
static esp_lcd_panel_handle_t    s_lcd_panel = NULL;

/* Settings globals defined in zigbee.c, read here for timeout checks */
extern uint16_t g_screen_timeout_sec;
extern uint8_t  g_dim_level;
extern bool     g_deep_sleep_enabled;
extern uint16_t g_sleep_timeout_sec;
extern uint8_t  g_night_brightness;
extern bool     g_night_mode;
extern uint8_t  g_backlight_level;

lv_display_t *display_init(void)
{
    /*
     * Backlight — LEDC PWM (replaces the original GPIO on/off).
     * Keep off until UI is fully built to avoid showing garbage frames.
     */
    backlight_init();
    backlight_set(0);

    /* ---- SPI Bus ---- */
    spi_bus_config_t bus = {
        .mosi_io_num     = PIN_LCD_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = PIN_LCD_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_H_RES * LCD_DRAW_BUF_LINES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    /* ---- LCD panel IO (SPI) ---- */
    esp_lcd_panel_io_handle_t io  = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num        = PIN_LCD_DC,
        .cs_gpio_num        = PIN_LCD_CS,
        .pclk_hz            = LCD_SPI_CLK_HZ,
        .lcd_cmd_bits       = 8,
        .lcd_param_bits     = 8,
        .spi_mode           = LCD_SPI_MODE,
        .trans_queue_depth  = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io));

    /* ---- ST7789V2 panel ---- */
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, LCD_OFFSET_X, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    /* Save handles for sleep/wake in display_timeout_task() */
    s_lcd_io    = io;
    s_lcd_panel = panel;
    ESP_LOGI(TAG, "ST7789V2 panel ready");

    /* ---- I2C bus (CST816 touch) ---- */
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_bus_config_t i2c_bus_cfg = {
        .clk_source              = I2C_CLK_SRC_DEFAULT,
        .i2c_port                = I2C_NUM_0,
        .scl_io_num              = PIN_TOUCH_SCL,
        .sda_io_num              = PIN_TOUCH_SDA,
        .glitch_ignore_cnt       = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus));

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_cfg.scl_speed_hz = TOUCH_I2C_CLK_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io));

    esp_lcd_touch_handle_t tp = NULL;
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = PIN_TOUCH_RST,
        .int_gpio_num = PIN_TOUCH_INT,     /* GPIO_NUM_NC if not wired        */
        .levels          = {.reset = 0, .interrupt = 0},
        .flags           = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
        .interrupt_callback = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &tp));
    ESP_LOGI(TAG, "CST816 touch ready");

    /* ---- LVGL port ---- */
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle    = io,
        .panel_handle = panel,
        .buffer_size  = LCD_H_RES * LCD_DRAW_BUF_LINES,
        .double_buffer = true,
        .hres         = LCD_H_RES,
        .vres         = LCD_V_RES,
        .monochrome   = false,
        .rotation     = {.swap_xy = false, .mirror_x = false, .mirror_y = false},
        .flags        = {.buff_dma = true, .swap_bytes = true},
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);

    const lvgl_port_touch_cfg_t touch_cfg = {.disp = disp, .handle = tp};
    lvgl_port_add_touch(&touch_cfg);

    /* Configure long-press time for tiles (default may be too slow) */
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev) {
        lv_indev_set_long_press_time(indev, TILE_LONG_PRESS_MS);
    }

    g_lvgl_ready = true;

    ESP_LOGI(TAG, "LVGL port ready (display + touch registered)");

    return disp;
}

/* ============================================================
   SCREEN TIMEOUT TASK
   ============================================================
   State machine:
     ACTIVE  — backlight at user level, touch resets the timer
     DIMMED  — backlight at dim_level after screen_timeout_sec
     SLEEP   — backlight fully off after sleep_timeout_sec
              (if deep_sleep_enabled → esp_deep_sleep_start if INT wired)
============================================================ */
typedef enum { SCREEN_ACTIVE, SCREEN_DIMMED, SCREEN_SLEEP } screen_state_t;

/* ST7789 commands for hardware sleep */
#define ST7789_CMD_SLPIN   0x10
#define ST7789_CMD_SLPOUT  0x11

static void lcd_enter_sleep(void)
{
    if (s_lcd_panel) esp_lcd_panel_disp_on_off(s_lcd_panel, false); /* DISPOFF */
    if (s_lcd_io)    esp_lcd_panel_io_tx_param(s_lcd_io, ST7789_CMD_SLPIN, NULL, 0);
    ESP_LOGI(TAG, "LCD HW sleep (SLPIN)");
}

static void lcd_exit_sleep(void)
{
    if (s_lcd_io)    esp_lcd_panel_io_tx_param(s_lcd_io, ST7789_CMD_SLPOUT, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));  /* ST7789 needs 120 ms after SLPOUT */
    if (s_lcd_panel) esp_lcd_panel_disp_on_off(s_lcd_panel, true);  /* DISPON  */
    ESP_LOGI(TAG, "LCD HW wake (SLPOUT)");
}

void display_timeout_task(void *arg)
{
    screen_state_t state        = SCREEN_ACTIVE;
    TickType_t     last_seen    = xTaskGetTickCount();
    g_screen_active = true;

    /* Wait until backlight is fully ready (display_init() may still be settling) */
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        /* Poll less often when screen is sleeping — saves CPU cycles */
        vTaskDelay(pdMS_TO_TICKS(
            state == SCREEN_SLEEP ? SLEEP_CHECK_INTERVAL_MS * 5
                                  : SLEEP_CHECK_INTERVAL_MS));

        TickType_t now    = xTaskGetTickCount();
        TickType_t idle   = now - s_last_touch_tick;     /* volatile — tiles.c   */
        uint32_t   idle_s = (uint32_t)(idle / configTICK_RATE_HZ);

        uint8_t  target_level = g_night_mode ? g_night_brightness : g_backlight_level;
        uint16_t screen_to    = g_screen_timeout_sec;
        uint16_t sleep_to     = g_sleep_timeout_sec;

        switch (state) {
        case SCREEN_ACTIVE:
            if (idle_s >= screen_to) {
                backlight_set(g_dim_level);
                state = SCREEN_DIMMED;
                g_screen_active = false;
                ESP_LOGI(TAG, "Screen dimmed (idle %us)", idle_s);
            }
            break;

        case SCREEN_DIMMED:
            /* Touch detected — wake back up */
            if (s_last_touch_tick != last_seen || idle_s < screen_to) {
                backlight_set(target_level);
                state = SCREEN_ACTIVE;
                g_screen_active = true;
                ESP_LOGI(TAG, "Screen woken");
                break;
            }
            if (idle_s >= sleep_to) {
                backlight_set(0);
                lcd_enter_sleep();             /* DISPOFF + SLPIN → ~10 µA LCD power */
                state = SCREEN_SLEEP;
                ESP_LOGI(TAG, "Screen off (sleep, idle %us)", idle_s);
                /* Note: Light sleep disabled — was blocking flash/Zigbee/toggle.
                 * LCD hardware sleep (SLPIN) saves most power anyway (~10 µA). */
            }
            break;

        case SCREEN_SLEEP:
            /* Any touch wakes screen */
            if (s_last_touch_tick != last_seen) {
                lcd_exit_sleep();              /* SLPOUT + 120 ms + DISPON */
                backlight_set(target_level);
                state = SCREEN_ACTIVE;
                g_screen_active = true;
                ESP_LOGI(TAG, "Screen woken from sleep");
            }
            break;
        }

        last_seen = s_last_touch_tick;
    }
}

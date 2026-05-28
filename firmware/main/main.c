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
 * main.c -- Kommando Panel firmware entry point.
 */

#include "config.h"
#include "backlight.h"
#include "tiles.h"
#include "commands.h"
#include "display.h"
#include "ui_panel.h"
#include "ui_events.h"
#include "zigbee.h"
#include "led.h"
#include "button.h"
#include "power.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_zigbee_core.h"
#include "esp_lvgl_port.h"

static const char *TAG = "main";

/* ============================================================
   ESP32 PLATFORM CALLBACK IMPLEMENTATIONS
   ============================================================ */

static bool impl_is_screen_active(void)       { return g_screen_active; }
static bool impl_is_screen_hw_sleeping(void)  { return g_screen_hw_sleeping; }

static int impl_get_backlight_pct(void)
{
    return (int)((g_backlight_level * 100) / 255);
}

static void impl_brightness_apply(uint8_t pct)
{
    g_backlight_level = (uint8_t)((pct * 255) / 100);
    backlight_set(g_backlight_level);
}

static void impl_brightness_report(uint8_t pct)
{
    zb_report_backlight_level_async((uint8_t)((pct * 255) / 100));
}

static const ui_platform_cbs_t s_ui_cbs = {
    .is_screen_active      = impl_is_screen_active,
    .is_screen_hw_sleeping = impl_is_screen_hw_sleeping,
    .on_screen_touch       = screen_wake,
    .on_tile_tap           = tiles_on_tap,
    .on_tile_hold          = tiles_on_hold,
    .on_dim_change         = tile_slider_level_cb,
    .get_backlight_pct     = impl_get_backlight_pct,
    .on_brightness_apply   = impl_brightness_apply,
    .on_brightness_report  = impl_brightness_report,
};

/* ---- LCD + UI Task ---- */

/*
 * Initialises display hardware, builds the LVGL UI, turns on the backlight,
 * then runs the screen-timeout state machine in an infinite loop.
 */
static void lcd_task(void *arg)
{
    lv_display_t *disp = display_init();

    /* Bind platform callbacks before building the UI */
    ui_panel_init(&s_ui_cbs);

    /* Build UI while holding the LVGL lock */
    vTaskDelay(pdMS_TO_TICKS(100));
    if (lvgl_port_lock(0)) {
        ui_panel_build(disp);
        lvgl_port_unlock();
    }

    /*
     * Re-apply NVS tile config now that LVGL objects exist.
     * The initial load in app_main ran before display_init() so
     * tile structs had no widget pointers yet.
     */
    if (lvgl_port_lock(0)) {
        load_tile_config_from_nvs();
        lvgl_port_unlock();
    }

    /* Apply startup backlight (respects night mode) */
    if (g_night_mode) {
        backlight_set(g_night_brightness);
        if (lvgl_port_lock(0)) {
            ui_panel_set_night_mode(true);
            lvgl_port_unlock();
        }
    } else {
        backlight_set(g_backlight_level);
    }

    /* Screen-timeout state machine -- never returns */
    display_timeout_task(NULL);
}

/* ============================================================
   APP MAIN
   ============================================================ */
void app_main(void)
{
    ESP_LOGI(TAG, "Kommando firmware v%s", FW_VERSION_STRING);

    /* Latch the wake cause before anything resets it. Used below to skip
     * cosmetic startup when we resumed from a touch wake. */
    power_log_wakeup_cause();

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Restore persisted settings (backlight level, timeouts, night mode, ...) */
    load_settings_from_nvs();

    /* Status LED -- pairing animation until network joins, but skip it
     * if we just woke from deep sleep (network state is restored from
     * NVS by the Zigbee stack; flashing the LED again is just noise). */
    ESP_ERROR_CHECK(led_init());
    if (!power_woke_from_deep_sleep()) {
        led_set_pattern(LED_PATTERN_PAIRING);
    } else {
        led_set_pattern(LED_PATTERN_OFF);
    }

    /* Physical button (short press = LED mode, long press = re-pair) */
    button_init();

    /* Tile infrastructure: create event queue, zero s_tiles[] */
    tiles_init();

    /* Zigbee radio platform */
    esp_zb_platform_config_t zb_cfg = {
        .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
        .host_config  = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&zb_cfg));

    ESP_LOGI(TAG, "Starting tasks");
    xTaskCreate(zigbee_task,      "zb_task",     TASK_ZB_STACK,     NULL, TASK_ZB_PRIO,     NULL);
    xTaskCreate(tile_report_task, "tile_report",  TASK_REPORT_STACK, NULL, TASK_REPORT_PRIO, NULL);
    xTaskCreate(lcd_task,         "lcd_task",     TASK_LCD_STACK,    NULL, TASK_LCD_PRIO,    NULL);
}

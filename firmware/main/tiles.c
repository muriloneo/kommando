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
 * tiles.c — ESP32-specific tile dispatch layer.
 *
 * Owns the FreeRTOS event queue, per-tile debounce timers, and the Zigbee
 * report task. Exposes platform callback implementations (tiles_on_tap,
 * tiles_on_hold, tile_slider_level_cb, screen_wake) that are registered into
 * the ui_platform_cbs_t struct in main.c.
 *
 * The tile state array (s_tiles[]) and ui_tile_t descriptor are in
 * ui_state.c / ui_state.h — no platform dependencies there.
 */

#include "tiles.h"
#include "backlight.h"
#include "config.h"
#include "display.h"
#include "zigbee.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_zigbee_core.h"

/* Forward-declare the Zigbee send function defined in zigbee.c */
void zb_send_tile_state(uint8_t param);

static const char *TAG = "tiles";

/* ---- FreeRTOS state (declared extern in tiles.h) ---- */
QueueHandle_t        s_tile_event_queue = NULL;
zb_tile_evt_t        s_zb_evt_buf[ZB_EVT_RING_SIZE];
int                  s_zb_evt_index = 0;

/* ---- Screen inactivity state (shared with display.c) ---- */
volatile TickType_t  s_last_touch_tick = 0;

static void enqueue_tile_event(const tile_event_t *evt)
{
    if (xQueueSend(s_tile_event_queue, evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full — dropping Tile[%d] type=%d", evt->id, (int)evt->type);
    }
}

/* ---- DIM slider debounce (200 ms) ---- */
#define DIM_DEBOUNCE_MS  200

static TimerHandle_t s_dim_timers[MAX_TILES];
static uint8_t       s_dim_pending_level[MAX_TILES];
static bool          s_dim_pending[MAX_TILES];

static void dim_debounce_cb(TimerHandle_t timer)
{
    int tile_id = (int)(uintptr_t)pvTimerGetTimerID(timer);
    if (tile_id < 0 || tile_id >= MAX_TILES) return;
    if (!s_dim_pending[tile_id]) return;

    s_dim_pending[tile_id] = false;
    uint8_t level = s_dim_pending_level[tile_id];

    tile_event_t evt = {
        .id    = tile_id,
        .type  = TILE_EVT_DIM,
        .state = s_tiles[tile_id].state,
        .value = level,
    };
    enqueue_tile_event(&evt);
    ESP_LOGI(TAG, "Tile[%d] dim debounced -> %u", tile_id, level);
}

void tiles_init(void)
{
    /* s_tiles[] is zero-initialised by the C runtime (defined in ui_state.c).
     * Set the id field and name for each slot. */
    for (int i = 0; i < MAX_TILES; i++) {
        s_tiles[i].id = i;
        snprintf(s_tiles[i].name, sizeof(s_tiles[i].name), "Tile_%d", i);
    }

    s_tile_event_queue = xQueueCreate(TILE_EVENT_QUEUE_SIZE, sizeof(tile_event_t));
    configASSERT(s_tile_event_queue);

    s_last_touch_tick = xTaskGetTickCount();

    ESP_LOGI(TAG, "Event queue ready (%d tiles)", MAX_TILES);

    /* Create per-tile debounce timers for the DIM slider */
    for (int i = 0; i < MAX_TILES; i++) {
        s_dim_pending[i] = false;
        s_dim_timers[i] = xTimerCreate(
            "dim_deb", pdMS_TO_TICKS(DIM_DEBOUNCE_MS),
            pdFALSE,  /* one-shot */
            (void *)(uintptr_t)i,
            dim_debounce_cb);
        configASSERT(s_dim_timers[i]);
    }
}

void screen_wake(void)
{
    s_last_touch_tick = xTaskGetTickCount();

    /* If screen is dimmed, immediately restore user backlight to avoid flicker */
    if (!g_screen_active) {
        uint8_t target_level = g_night_mode ? g_night_brightness : g_backlight_level;
        backlight_set(target_level);
        g_screen_active = true;
    }
}

/* ---- LVGL touch callback ---- */
/* ============================================================
   PLATFORM CALLBACK IMPLEMENTATIONS (registered via ui_platform_cbs_t)
   ============================================================ */

void tiles_on_tap(int tile_id, bool new_state)
{
    tile_event_t evt = {
        .id    = tile_id,
        .type  = TILE_EVT_TAP,
        .state = new_state,
        .value = s_tiles[tile_id].level,
    };
    enqueue_tile_event(&evt);
}

void tiles_on_hold(int tile_id, uint8_t current_level)
{
    tile_event_t evt = {
        .id    = tile_id,
        .type  = TILE_EVT_HOLD,
        .state = s_tiles[tile_id].state,
        .value = current_level,
    };
    enqueue_tile_event(&evt);
    ESP_LOGI(TAG, "Tile[%d] hold enqueued (level=%u)", tile_id, current_level);
}

void tile_slider_level_cb(int tile_id, uint8_t level)
{
    if (tile_id < 0 || tile_id >= MAX_TILES) return;
    if (!s_tiles[tile_id].dimmable) {
        ESP_LOGI(TAG, "Tile[%d] dim update ignored: not dimmable", tile_id);
        return;
    }

    s_tiles[tile_id].level    = level;
    s_dim_pending_level[tile_id] = level;
    s_dim_pending[tile_id]    = true;

    /* Reset debounce timer — fires after DIM_DEBOUNCE_MS of inactivity */
    xTimerReset(s_dim_timers[tile_id], 0);
}

/* ---- Tile report task ---- */
void tile_report_task(void *arg)
{
    (void)arg;
    tile_event_t evt;
    while (1) {
        if (xQueueReceive(s_tile_event_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (evt.type == TILE_EVT_TAP) {
            ESP_LOGI(TAG, "Reporting TAP Tile[%d] -> %s", evt.id, evt.state ? "ON" : "OFF");
        } else if (evt.type == TILE_EVT_HOLD) {
            ESP_LOGI(TAG, "Reporting HOLD Tile[%d]", evt.id);
        } else {
            ESP_LOGI(TAG, "Reporting DIM Tile[%d] -> %u", evt.id, evt.value);
        }

        int idx = s_zb_evt_index;
        s_zb_evt_index = (s_zb_evt_index + 1) % ZB_EVT_RING_SIZE;
        s_zb_evt_buf[idx].id    = evt.id;
        s_zb_evt_buf[idx].type  = evt.type;
        s_zb_evt_buf[idx].state = evt.state;
        s_zb_evt_buf[idx].value = evt.value;

        /* Schedule send in Zigbee task context after a short guard delay */
        esp_zb_lock_acquire(portMAX_DELAY);
        esp_zb_scheduler_alarm(zb_send_tile_state, (uint8_t)idx, ZB_REPORT_DELAY_MS);
        esp_zb_lock_release();
    }
}

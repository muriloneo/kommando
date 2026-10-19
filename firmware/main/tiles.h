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
 * tiles.h — ESP32-specific tile event dispatch and FreeRTOS infrastructure.
 *
 * The tile state struct (ui_tile_t) and shared array (s_tiles[]) live in
 * ui_state.h / ui_state.c so they can be included without RTOS headers.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "config.h"
#include "ui/ui_state.h"   /* ui_tile_t, s_tiles[] */

/* ---- Internal event types for the touch→Zigbee queue ---- */
typedef enum {
    TILE_EVT_TAP = 0,
    TILE_EVT_HOLD,
    TILE_EVT_DIM,
} tile_evt_type_t;

typedef struct {
    int             id;
    tile_evt_type_t type;
    bool            state;   /* used by TAP */
    uint8_t         value;   /* used by DIM and HOLD */
} tile_event_t;

/* ---- Event buffer for esp_zb_scheduler_alarm() callbacks ---- */
typedef struct {
    int             id;
    tile_evt_type_t type;
    bool            state;
    uint8_t         value;
} zb_tile_evt_t;

extern QueueHandle_t         s_tile_event_queue;
extern volatile TickType_t   s_last_touch_tick;
extern zb_tile_evt_t         s_zb_evt_buf[ZB_EVT_RING_SIZE];
extern int                   s_zb_evt_index;

/**
 * tiles_init — Create the FreeRTOS event queue and per-tile debounce timers.
 * Call from app_main() before starting any tasks.
 */
void tiles_init(void);

/**
 * screen_wake — Reset the inactivity timestamp and restore backlight.
 * Registered as the on_screen_touch platform callback.
 */
void screen_wake(void);

/**
 * tiles_on_tap / tiles_on_hold — ui_platform_cbs_t implementations.
 * Enqueue the appropriate tile_event_t onto the FreeRTOS queue.
 */
void tiles_on_tap(int tile_id, bool new_state);
void tiles_on_hold(int tile_id, uint8_t current_level);

/**
 * tile_slider_level_cb — on_dim_change platform callback implementation.
 * Updates per-tile level and resets the debounce timer.
 */
void tile_slider_level_cb(int tile_id, uint8_t level);

/**
 * tile_report_task — FreeRTOS task body.
 * Drains the event queue and schedules Zigbee reports via the scheduler alarm.
 */
void tile_report_task(void *arg);

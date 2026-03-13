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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "config.h"

/* ---- Tile descriptor ---- */
typedef struct {
    int        id;                        /* 0–5                                  */
    lv_obj_t  *tile;                      /* LVGL container object                */
    lv_obj_t  *icon_label;                /* LVGL label — icon glyph              */
    lv_obj_t  *name_label;                /* LVGL label — friendly name           */
    lv_obj_t  *state_label;               /* LVGL label — "ON" / "OFF" / ""       */
    lv_obj_t  *dimmer_badge;              /* LVGL dimmer indicator (small dot)     */
    bool       state;                     /* Current logical ON/OFF state          */
    bool       configured;                /* True once HA has sent a C: command    */
    bool       dimmable;                  /* True if HA says this tile supports dimming */
    uint8_t    level;                     /* 0..100 dim level                      */
    bool       suppress_next_click;       /* Prevent click after long press        */
    char       name[TILE_NAME_MAX_LEN];   /* Friendly name string                  */
} ui_tile_t;

typedef enum {
    TILE_EVT_TAP = 0,
    TILE_EVT_HOLD,
    TILE_EVT_DIM,
} tile_evt_type_t;

/* ---- Event passed through the touch→Zigbee queue ---- */
typedef struct {
    int             id;
    tile_evt_type_t type;
    bool            state;   /* used by TAP */
    uint8_t         value;   /* used by DIM, and current level for HOLD */
} tile_event_t;

/* ---- Event buffer entry for esp_zb_scheduler_alarm() callbacks ---- */
typedef struct {
    int             id;
    tile_evt_type_t type;
    bool            state;
    uint8_t         value;
} zb_tile_evt_t;

/* Global tile array — read by commands.c and ui_panel.c */
extern ui_tile_t       s_tiles[MAX_TILES];
extern QueueHandle_t   s_tile_event_queue;

/* Inactivity timestamp — updated by tile_event_cb(), read by display.c */
extern volatile TickType_t s_last_touch_tick;

/* Ring buffer used by Zigbee report scheduling */
extern zb_tile_evt_t   s_zb_evt_buf[ZB_EVT_RING_SIZE];
extern int             s_zb_evt_index;

/*
 * tiles_init — Initialise the tile array and create the event queue.
 * Must be called from app_main() before any task is started.
 */
void tiles_init(void);

/*
 * tile_event_cb — LVGL event callback; register on each tile with
 * lv_obj_add_event_cb(tile, tile_event_cb, LV_EVENT_CLICKED, &s_tiles[i]).
 * Toggles tile state and enqueues a tile_event_t.
 */
void tile_event_cb(lv_event_t *e);

/*
 * screen_wake — reset inactivity timer to keep screen active.
 */
void screen_wake(void);

/*
 * ui_panel_update_dimmer_badge — Show/hide dimmer indicator on tile.
 * Called when tile.dimmable state changes (from HA D: command).
 */
void ui_panel_update_dimmer_badge(int tile_id, bool dimmable);

/*
 * tile_report_task — FreeRTOS task body.
 * Dequeues tile_event_t items and schedules zb_send_tile_state() via the
 * Zigbee scheduler alarm so the send runs in the Zigbee task context.
 */
void tile_report_task(void *arg);

/*
 * Called by UI slider overlay when user changes dim value.
 */
void tile_slider_level_cb(int tile_id, uint8_t level);

/*
 * screen_wake — Reset the inactivity timer and restore backlight.
 * Called by tile_event_cb() and by the button driver on any press.
 */
void screen_wake(void);

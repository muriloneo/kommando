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
 * ui_state.h — Tile state descriptor and shared tile array.
 *
 * No platform-specific (ESP32 / RTOS) dependencies.
 * Safe to include in ui_panel.c, tiles.c, commands.c, and the simulator.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "ui_config.h"

/* ---- Tile descriptor ---- */
typedef struct {
    int        id;                        /* 0 – (MAX_TILES-1)                     */
    lv_obj_t  *tile;                      /* LVGL container object                 */
    lv_obj_t  *icon_label;                /* LVGL label — icon glyph               */
    lv_obj_t  *name_label;                /* LVGL label — friendly name            */
    lv_obj_t  *state_label;               /* LVGL label — "ON" / "OFF" / ""        */
    lv_obj_t  *dimmer_badge;              /* Small dot: dimmable capability badge  */
    bool       state;                     /* Current logical ON/OFF state          */
    bool       configured;                /* True once HA sent a C: command        */
    bool       dimmable;                  /* True if tile supports dimming         */
    uint8_t    level;                     /* Current dim level 0–100               */
    bool       suppress_next_click;       /* Prevent click after long press        */
    char       name[TILE_NAME_MAX_LEN];   /* Friendly name string                  */
} ui_tile_t;

/* Global tile array — shared by ui_panel.c, tiles.c, and commands.c */
extern ui_tile_t s_tiles[MAX_TILES];

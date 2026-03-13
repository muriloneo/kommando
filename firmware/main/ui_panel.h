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

#include "lvgl.h"
#include <stdbool.h>

/**
 * Build the full panel UI on the currently active screen.
 * Must be called while holding the LVGL port lock.
 *
 * @param disp  LVGL display handle returned by display_init().
 */
void ui_panel_build(lv_display_t *disp);

/**
 * Switch the panel between normal and night-mode colour scheme.
 * Night mode dims tile "on" colour to reduce glare.
 * Must be called while holding the LVGL port lock.
 *
 * @param night_on  true = enable night colour scheme.
 */
void ui_panel_set_night_mode(bool night_on);

/**
 * Show dimmer overlay for a tile (opened on long-press).
 */
void ui_panel_show_dimmer(int tile_id, const char *name, uint8_t level);

/**
 * Update dimmer badge visibility on a tile.
 * Called when tile.dimmable state changes (from HA D: command).
 *
 * @param tile_id   Tile index (0-5)
 * @param dimmable  true = show orange dot, false = hide
 */
void ui_panel_update_dimmer_badge(int tile_id, bool dimmable);

/**
 * Hide dimmer overlay.
 */
void ui_panel_hide_dimmer(void);

/**
 * Return the current "tile on" background colour.
 * Accounts for whether night mode is active.
 * Used by commands.c when applying an S: state-change command.
 */
lv_color_t ui_panel_get_on_color(void);

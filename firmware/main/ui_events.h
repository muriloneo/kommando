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
 * ui_events.h — Platform abstraction callbacks for the Kommando UI layer.
 *
 * Register a ui_platform_cbs_t with ui_panel_init() before ui_panel_build().
 *
 * On ESP32:   implement using backlight, Zigbee, and FreeRTOS primitives.
 * On desktop: implement with printf / SDL2 stubs (see simulator/main.c).
 *
 * Adding animations later: register an on_tile_anim callback here to
 * trigger platform-specific or purely visual animation sequences.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* ---- Screen state queries ------------------------------------------ */

    /** Returns true when the screen is fully lit (not dimmed / sleeping). */
    bool (*is_screen_active)(void);

    /** Returns true when the LCD is in hardware sleep state. */
    bool (*is_screen_hw_sleeping)(void);

    /** Any touch gesture — resets the inactivity timer / restores backlight. */
    void (*on_screen_touch)(void);

    /* ---- Tile user actions --------------------------------------------- */

    /**
     * A tile was tapped (toggled).
     * new_state: true = ON, false = OFF.
     * The UI already updated its visual state before calling this.
     */
    void (*on_tile_tap)(int tile_id, bool new_state);

    /**
     * A tile was long-pressed; the dimmer overlay has been opened.
     * current_level: the tile's current dim level (0–100).
     */
    void (*on_tile_hold)(int tile_id, uint8_t current_level);

    /**
     * The dimmer slider moved to a new level (0–100).
     * Debouncing for network reporting is the platform's responsibility.
     */
    void (*on_dim_change)(int tile_id, uint8_t level_pct);

    /* ---- Panel brightness ---------------------------------------------- */

    /** Returns the current panel brightness in percent (0–100). */
    int (*get_backlight_pct)(void);

    /**
     * Panel brightness slider moved — apply hardware change immediately.
     * Called on every slider update for smooth visual feedback.
     */
    void (*on_brightness_apply)(uint8_t level_pct);

    /**
     * Panel brightness settled after dragging — report to network.
     * Called after a short idle window via the built-in LVGL debounce timer.
     */
    void (*on_brightness_report)(uint8_t level_pct);

} ui_platform_cbs_t;

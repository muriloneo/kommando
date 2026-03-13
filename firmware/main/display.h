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

#include "esp_err.h"
#include "lvgl.h"

/**
 * Initialise the SPI LCD, I2C touch controller, and LVGL port.
 * Returns the LVGL display handle. Blocks until hardware is ready.
 * Must be called before any LVGL drawing.
 */
lv_display_t *display_init(void);

/**
 * True when screen is fully active (not dimmed or sleeping).
 * Checked by tile_event_cb() to suppress tile toggles on wake taps.
 */
extern volatile bool g_screen_active;

/**
 * True once LVGL port has been initialized and is safe to lock.
 */
extern volatile bool g_lvgl_ready;

/**
 * Screen-timeout monitor task (long-running FreeRTOS task).
 * Dims the backlight after SCREEN_TIMEOUT_DEFAULT_SEC seconds of inactivity,
 * then turns off according to g_screen_timeout_sec / g_dim_level settings.
 *
 * arg: ignored
 */
void display_timeout_task(void *arg);

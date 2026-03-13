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

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/*
 * backlight_init — Configure LEDC timer and channel, set default brightness.
 * Must be called before any other backlight_* function.
 */
esp_err_t backlight_init(void);

/*
 * backlight_set — Set brightness immediately.
 * level: 0 = fully off, 255 = full brightness.
 */
void backlight_set(uint8_t level);

/*
 * backlight_get — Return the last level passed to backlight_set().
 */
uint8_t backlight_get(void);

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
#include "esp_err.h"

typedef enum {
    LED_PATTERN_OFF = 0,
    LED_PATTERN_SOLID_ON,
    LED_PATTERN_PAIRING,
    LED_PATTERN_IDLE_BREATHING,
    LED_PATTERN_ERROR,
} led_pattern_t;

esp_err_t led_init(void);
void led_set_pattern(led_pattern_t pattern);

void led_set_breath_speed(int ms);
bool led_is_available(void);

void led_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void led_clear_rgb_override(void);

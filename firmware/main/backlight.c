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

#include "backlight.h"
#include "config.h"

#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

static const char *TAG = "backlight";
static uint8_t s_current_level = BL_LEVEL_DEFAULT;

esp_err_t backlight_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = BL_LEDC_SPEED_MODE,
        .duty_resolution = BL_PWM_RESOLUTION,
        .timer_num       = BL_LEDC_TIMER,
        .freq_hz         = BL_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "LEDC timer config failed");

    ledc_channel_config_t ch_cfg = {
        .gpio_num   = PIN_LCD_BL,
        .speed_mode = BL_LEDC_SPEED_MODE,
        .channel    = BL_LEDC_CHANNEL,
        .timer_sel  = BL_LEDC_TIMER,
        .duty       = BL_LEVEL_DEFAULT,
        .hpoint     = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "LEDC channel config failed");

    ESP_LOGI(TAG, "Backlight PWM ready — GPIO%d, %d Hz, level=%d",
             PIN_LCD_BL, BL_PWM_FREQ_HZ, BL_LEVEL_DEFAULT);
    return ESP_OK;
}

void backlight_set(uint8_t level)
{
    s_current_level = level;
    /* Backlight circuit is active-low: duty 0 = fully on, 255 = off.
       Invert so that level 255 = brightest for the caller. */
    ledc_set_duty(BL_LEDC_SPEED_MODE, BL_LEDC_CHANNEL, 255 - level);
    ledc_update_duty(BL_LEDC_SPEED_MODE, BL_LEDC_CHANNEL);
}

uint8_t backlight_get(void)
{
    return s_current_level;
}

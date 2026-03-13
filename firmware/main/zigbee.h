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

/* ---- Device-setting globals (defined in zigbee.c) ---- */
extern uint8_t  g_backlight_level;      /* ZB attr 0x0010 */
extern uint16_t g_screen_timeout_sec;   /* ZB attr 0x0011 */
extern uint8_t  g_dim_level;            /* ZB attr 0x0012 */
extern bool     g_night_mode;           /* ZB attr 0x0013 */
extern uint8_t  g_night_brightness;     /* ZB attr 0x0014 */
extern bool     g_deep_sleep_enabled;   /* ZB attr 0x0015 */
extern uint16_t g_sleep_timeout_sec;    /* ZB attr 0x0016 */

/**
 * FreeRTOS task entry point — calls zigbee_init() then runs the scheduler
 * loop. Should be created with a stack of at least TASK_ZB_STACK bytes.
 */
void zigbee_task(void *arg);

/**
 * Send a tile-tap event to the coordinator via the OnOff cluster.
 * Called through esp_zb_scheduler_alarm() from tiles.c.
 *
 * @param param  Index into the internal zb_evt_buf ring buffer.
 */
void zb_send_tile_state(uint8_t param);

/**
 * Report a new backlight level from the device UI to the coordinator.
 * Updates local state, persists to NVS, and sends an attribute report.
 */
void zb_report_backlight_level(uint8_t level);

/**
 * Async-safe wrapper for UI thread usage.
 * Schedules backlight reporting to run in Zigbee scheduler context.
 */
void zb_report_backlight_level_async(uint8_t level);

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

/* ---- Command processing ---- */

/*
 * process_ha_command — Parse and execute a null-terminated command string.
 * Must be called with the LVGL port lock held (lvgl_port_lock).
 * Returns ESP_ERR_INVALID_ARG if the payload is malformed.
 */
esp_err_t process_ha_command(char *payload);

/*
 * should_drop_duplicate_cfg — Returns true and logs a warning if the given
 * C: payload is identical to the last one received for the same tile.
 * Prevents infinite Z2M echo→HA→device loops.
 */
bool should_drop_duplicate_cfg(const char *payload);

/* ---- NVS — Tile config ---- */

/*
 * save_tile_to_nvs — Persist the full C: command string for one tile.
 */
void save_tile_to_nvs(int tile_index, const char *cmd_str);

/*
 * load_tile_config_from_nvs — Restore all previously saved tile configs.
 * Call from app_main() before LVGL is ready (s_disp == NULL guard inside).
 */
void load_tile_config_from_nvs(void);

/* ---- NVS — Device settings ---- */

/*
 * load_settings_from_nvs — Populate the device-settings globals from NVS.
 * Called from app_main() once at boot.
 */
void load_settings_from_nvs(void);

void save_setting_u8(const char *key, uint8_t value);
void save_setting_u16(const char *key, uint16_t value);

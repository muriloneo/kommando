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

#include "commands.h"
#include "tiles.h"
#include "backlight.h"
#include "ui_panel.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "display.h"

static const char *TAG = "commands";

/* ---- Duplicate detection cache: one entry per tile ---- */
static char s_last_cfg_cmd[MAX_TILES][64];

/* ---- Icon string → LVGL symbol lookup table ---- */
typedef struct {
    const char *name;    /* String sent in the C: command */
    const char *symbol;  /* LVGL symbol UTF-8 string      */
} icon_entry_t;

/*
 * To add new icons: append a row here and add the matching option to the
 * blueprint's tile_N_icon selector.  No other code changes needed.
 */
static const icon_entry_t ICON_TABLE[] = {
    { "bulb",  LV_SYMBOL_CHARGE  },
    { "wifi",  LV_SYMBOL_WIFI    },
    { "home",  LV_SYMBOL_HOME    },
    { "power", LV_SYMBOL_POWER   },
    { "bell",  LV_SYMBOL_BELL    },
    { "up",    LV_SYMBOL_UP      },
    { "down",  LV_SYMBOL_DOWN    },
    { "ok",    LV_SYMBOL_OK      },
    { "close", LV_SYMBOL_CLOSE   },
    { "audio", LV_SYMBOL_AUDIO   },
};
static const int ICON_TABLE_SIZE = sizeof(ICON_TABLE) / sizeof(ICON_TABLE[0]);

static const char *resolve_icon(const char *icon_str)
{
    if (!icon_str) return LV_SYMBOL_POWER;
    for (int i = 0; i < ICON_TABLE_SIZE; i++) {
        if (strcmp(icon_str, ICON_TABLE[i].name) == 0) {
            return ICON_TABLE[i].symbol;
        }
    }
    if (strstr(icon_str, "mdi:") == icon_str) {
        return LV_SYMBOL_POWER; /* Generic fallback for MDI-style names */
    }
    return LV_SYMBOL_POWER; /* Fallback                                   */
}

/* ============================================================
   COMMAND PROCESSING
   ============================================================ */

esp_err_t process_ha_command(char *payload)
{
    ESP_LOGI(TAG, "CMD: [%s]", payload);

    char tmp[64];
    strncpy(tmp, payload, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *saveptr;
    char *cmd    = strtok_r(tmp, ":", &saveptr);
    char *id_str = strtok_r(NULL, ":", &saveptr);

    if (!cmd || !id_str) {
        ESP_LOGW(TAG, "CMD: malformed payload");
        return ESP_ERR_INVALID_ARG;
    }

    int id = atoi(id_str);
    if (id < 0 || id >= MAX_TILES) {
        ESP_LOGW(TAG, "CMD: tile id %d out of range", id);
        return ESP_ERR_INVALID_ARG;
    }

    /* ----------------------------------------------------------
       C:<id>:<icon>:<name> — Configure tile
       ---------------------------------------------------------- */
    if (strcmp(cmd, "C") == 0) {
        char *icon_str = strtok_r(NULL, ":", &saveptr);
        char *name_str = strtok_r(NULL, "",  &saveptr); /* Rest of string */

        if (!icon_str) {
            ESP_LOGW(TAG, "CMD C: missing icon for tile %d", id);
            return ESP_ERR_INVALID_ARG;
        }
        if (!name_str) name_str = "";

        const char *symbol = resolve_icon(icon_str);
        bool was_configured = s_tiles[id].configured;
        bool keep_state = s_tiles[id].state;

        /* Update LVGL objects only when LVGL is ready */
        if (s_tiles[id].tile != NULL && g_lvgl_ready && lvgl_port_lock(0)) {
            lv_label_set_text(s_tiles[id].icon_label,  symbol);
            lv_label_set_text(s_tiles[id].name_label,  name_str);
            lv_obj_set_style_text_color(s_tiles[id].icon_label, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_text_color(s_tiles[id].name_label, lv_color_white(), LV_PART_MAIN);
            lv_color_t on_color = ui_panel_get_on_color();
            lv_obj_set_style_bg_color(s_tiles[id].tile,
                keep_state ? on_color : lv_color_make(COLOR_TILE_OFF),
                LV_PART_MAIN);
            lvgl_port_unlock();
        }

        s_tiles[id].configured = true;
        s_tiles[id].dimmable = false;
        s_tiles[id].level = 0;
        s_tiles[id].suppress_next_click = false;
        strncpy(s_tiles[id].name, name_str, TILE_NAME_MAX_LEN - 1);
        s_tiles[id].name[TILE_NAME_MAX_LEN - 1] = '\0';

        /* Clear dimmer badge on reconfigure */
        if (s_tiles[id].tile != NULL && g_lvgl_ready && lvgl_port_lock(0)) {
            ui_panel_update_dimmer_badge(id, false);
            lvgl_port_unlock();
        }

        ESP_LOGI(TAG, "Tile[%d] %s — icon='%s' name='%s'",
                 id,
                 was_configured ? "RECONFIGURED" : "CONFIGURED",
                 icon_str, s_tiles[id].name);
        return ESP_OK;
    }

    /* ----------------------------------------------------------
       S:<id>:<0|1> — Set visual state
       ---------------------------------------------------------- */
    if (strcmp(cmd, "S") == 0) {
        char *state_str = strtok_r(NULL, ":", &saveptr);
        if (!state_str) {
            ESP_LOGW(TAG, "CMD S: missing state for tile %d", id);
            return ESP_ERR_INVALID_ARG;
        }

        if (!s_tiles[id].configured) {
            ESP_LOGW(TAG, "CMD S: tile %d not yet configured — skipping", id);
            return ESP_OK;
        }

        bool new_state = (strcmp(state_str, "1") == 0);
        bool old_state = s_tiles[id].state;
        s_tiles[id].state = new_state;

        if (s_tiles[id].tile != NULL && g_lvgl_ready && lvgl_port_lock(0)) {
            /* Honor night-mode color via ui_panel getter */
            lv_color_t on_color = ui_panel_get_on_color();
            lv_obj_set_style_bg_color(s_tiles[id].tile,
                new_state ? on_color : lv_color_make(COLOR_TILE_OFF),
                LV_PART_MAIN);
            lvgl_port_unlock();
        }

        ESP_LOGI(TAG, "Tile[%d] '%s' %s → %s",
                 id, s_tiles[id].name,
                 old_state ? "ON" : "OFF",
                 new_state ? "ON" : "OFF");
        return ESP_OK;
    }

    /* ----------------------------------------------------------
       D:<id>:<0|1>:<0..100> — Configure dimmer support + level
       ---------------------------------------------------------- */
    if (strcmp(cmd, "D") == 0) {
        char *enabled_str = strtok_r(NULL, ":", &saveptr);
        char *level_str   = strtok_r(NULL, ":", &saveptr);
        if (!enabled_str || !level_str) {
            ESP_LOGW(TAG, "CMD D: missing enabled/level for tile %d", id);
            return ESP_ERR_INVALID_ARG;
        }

        int enabled = atoi(enabled_str);
        int level   = atoi(level_str);
        if (level < 0) level = 0;
        if (level > 100) level = 100;

        s_tiles[id].dimmable = (enabled != 0);
        s_tiles[id].level = (uint8_t)level;

        /* Update dimmer badge visibility on the tile */
        if (s_tiles[id].tile != NULL && g_lvgl_ready && lvgl_port_lock(0)) {
            ui_panel_update_dimmer_badge(id, s_tiles[id].dimmable);
            lvgl_port_unlock();
        }

        ESP_LOGI(TAG, "Tile[%d] dimmer -> enabled=%d level=%d",
                 id, s_tiles[id].dimmable ? 1 : 0, (int)s_tiles[id].level);
        return ESP_OK;
    }

    /* ----------------------------------------------------------
       L:<id>:<0..100> — Update dimmer level only
       ---------------------------------------------------------- */
    if (strcmp(cmd, "L") == 0) {
        char *level_str = strtok_r(NULL, ":", &saveptr);
        if (!level_str) {
            ESP_LOGW(TAG, "CMD L: missing level for tile %d", id);
            return ESP_ERR_INVALID_ARG;
        }

        int level = atoi(level_str);
        if (level < 0) level = 0;
        if (level > 100) level = 100;
        s_tiles[id].level = (uint8_t)level;

        ESP_LOGI(TAG, "Tile[%d] level -> %d", id, (int)s_tiles[id].level);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "CMD: unknown command '%s'", cmd);
    return ESP_ERR_INVALID_ARG;
}

bool should_drop_duplicate_cfg(const char *payload)
{
    if (!payload) return false;

    /* Only apply de-duplication to C: commands */
    char tmp[64];
    strncpy(tmp, payload, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *saveptr;
    char *cmd    = strtok_r(tmp, ":", &saveptr);
    char *id_str = strtok_r(NULL, ":", &saveptr);

    if (!cmd || !id_str || strcmp(cmd, "C") != 0) return false;

    int tile_id = atoi(id_str);
    if (tile_id < 0 || tile_id >= MAX_TILES) return false;

    if (strcmp(s_last_cfg_cmd[tile_id], payload) == 0) {
        ESP_LOGW(TAG, "Duplicate C: for Tile[%d] — ignored", tile_id);
        return true;
    }

    strncpy(s_last_cfg_cmd[tile_id], payload, sizeof(s_last_cfg_cmd[tile_id]) - 1);
    s_last_cfg_cmd[tile_id][sizeof(s_last_cfg_cmd[tile_id]) - 1] = '\0';
    return false;
}

/* ============================================================
   NVS — TILE CONFIG
   ============================================================ */

void save_tile_to_nvs(int tile_index, const char *cmd_str)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) return;

    char key[16];
    snprintf(key, sizeof(key), NVS_KEY_TILE_FMT, tile_index);
    nvs_set_str(nvs, key, cmd_str);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "NVS: tile[%d] saved", tile_index);
}

void load_tile_config_from_nvs(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "NVS: no saved tile config found");
        return;
    }

    for (int i = 0; i < MAX_TILES; i++) {
        char key[16];
        snprintf(key, sizeof(key), NVS_KEY_TILE_FMT, i);
        char buf[64];
        size_t len = sizeof(buf);
        if (nvs_get_str(nvs, key, buf, &len) == ESP_OK) {
            /* LVGL is not ready yet — process_ha_command guards against NULL tile */
            process_ha_command(buf);
            ESP_LOGI(TAG, "NVS: tile[%d] restored → %s", i, buf);
        }
    }
    nvs_close(nvs);
}

/* ============================================================
   NVS — DEVICE SETTINGS
   ============================================================ */

/* Forward-declared: set from zigbee.c or loaded by this function */
extern uint8_t  g_backlight_level;
extern uint16_t g_screen_timeout_sec;
extern uint8_t  g_dim_level;
extern bool     g_night_mode;
extern uint8_t  g_night_brightness;
extern bool     g_deep_sleep_enabled;
extern uint16_t g_sleep_timeout_sec;

void load_settings_from_nvs(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) return;

    uint8_t  u8;
    uint16_t u16;

    if (nvs_get_u8(nvs,  NVS_KEY_BACKLIGHT,      &u8)  == ESP_OK) g_backlight_level    = u8;
    if (nvs_get_u16(nvs, NVS_KEY_SCREEN_TIMEOUT,  &u16) == ESP_OK) g_screen_timeout_sec = u16;
    if (nvs_get_u8(nvs,  NVS_KEY_DIM_LEVEL,       &u8)  == ESP_OK) g_dim_level          = u8;
    if (nvs_get_u8(nvs,  NVS_KEY_NIGHT_MODE,      &u8)  == ESP_OK) g_night_mode         = (bool)u8;
    if (nvs_get_u8(nvs,  NVS_KEY_NIGHT_BL,        &u8)  == ESP_OK) g_night_brightness   = u8;
    if (nvs_get_u8(nvs,  NVS_KEY_DEEP_SLEEP_EN,   &u8)  == ESP_OK) g_deep_sleep_enabled = (bool)u8;
    if (nvs_get_u16(nvs, NVS_KEY_SLEEP_TIMEOUT,   &u16) == ESP_OK) g_sleep_timeout_sec  = u16;

    nvs_close(nvs);
    ESP_LOGI(TAG, "NVS: settings loaded (bl=%d, timeout=%ds, night=%d)",
             g_backlight_level, g_screen_timeout_sec, g_night_mode);
}

void save_setting_u8(const char *key, uint8_t value)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) return;
    nvs_set_u8(nvs, key, value);
    nvs_commit(nvs);
    nvs_close(nvs);
}

void save_setting_u16(const char *key, uint16_t value)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) return;
    nvs_set_u16(nvs, key, value);
    nvs_commit(nvs);
    nvs_close(nvs);
}

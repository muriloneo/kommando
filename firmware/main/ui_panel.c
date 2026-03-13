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

#include "ui_panel.h"
#include "tiles.h"    /* s_tiles[], tile_event_cb()                     */
#include "config.h"
#include "zigbee.h"
#include "backlight.h"
#include "display.h"  /* g_screen_active                                */

#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static lv_obj_t *s_dimmer_overlay = NULL;
static lv_obj_t *s_dimmer_title   = NULL;
static lv_obj_t *s_dimmer_value   = NULL;
static lv_obj_t *s_dimmer_slider  = NULL;
static int       s_dimmer_tile_id = -1;
static bool      s_dimmer_is_panel = false;
static lv_timer_t *s_dimmer_idle_timer = NULL;
static uint32_t  s_dimmer_last_activity = 0;
static TimerHandle_t s_panel_bl_report_timer = NULL;
static uint8_t    s_panel_bl_pending = BL_LEVEL_DEFAULT;

static const char *TAG = "ui_panel";

/* Current night-mode flag — kept here so ui_panel_get_on_color is accurate */
static bool s_night_mode = false;

static void dimmer_idle_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_dimmer_overlay) return;
    if (lv_obj_has_flag(s_dimmer_overlay, LV_OBJ_FLAG_HIDDEN)) return;

    uint32_t now = lv_tick_get();
    if ((now - s_dimmer_last_activity) >= DIMMER_IDLE_TIMEOUT_MS) {
        ui_panel_hide_dimmer();
    }
}

static void dimmer_touch_activity(void)
{
    screen_wake();
    s_dimmer_last_activity = lv_tick_get();
    if (!s_dimmer_idle_timer) {
        s_dimmer_idle_timer = lv_timer_create(dimmer_idle_timer_cb, 500, NULL);
    }
}

static void panel_backlight_report_timer_cb(TimerHandle_t t)
{
    (void)t;
    zb_report_backlight_level_async(s_panel_bl_pending);
}

static void dimmer_slider_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    if (!s_dimmer_slider || s_dimmer_tile_id < 0) return;

    int value = lv_slider_get_value(s_dimmer_slider);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    lv_label_set_text(s_dimmer_value, buf);
    dimmer_touch_activity();

    if (s_dimmer_is_panel) {
        /* Map 0..100% -> 0..255 backlight */
        uint8_t bl = (uint8_t)((value * 255) / 100);

        /* Apply locally immediately for smooth UI feedback */
        g_backlight_level = bl;
        backlight_set(bl);

        /* Debounce Zigbee reporting to avoid flooding APS queue */
        s_panel_bl_pending = bl;
        if (!s_panel_bl_report_timer) {
            s_panel_bl_report_timer = xTimerCreate(
                "panel_bl_rep",
                pdMS_TO_TICKS(150),
                pdFALSE, /* one-shot */
                NULL,
                panel_backlight_report_timer_cb);
            if (!s_panel_bl_report_timer) {
                ESP_LOGW(TAG, "Failed to create panel backlight debounce timer");
                return;
            }
        }

        xTimerChangePeriod(s_panel_bl_report_timer, pdMS_TO_TICKS(150), 0);
        xTimerReset(s_panel_bl_report_timer, 0);
    } else {
        tile_slider_level_cb(s_dimmer_tile_id, (uint8_t)value);
    }
}

static void dimmer_overlay_click_cb(lv_event_t *e)
{
    (void)e;
    ui_panel_hide_dimmer();
}

static void header_click_cb(lv_event_t *e)
{
    (void)e;
    
    /* Capture screen state before wake */
    bool was_active = g_screen_active;
    screen_wake();
    
    /* If waking from sleep/dim, suppress the brightness control action */
    if (!was_active) {
        return;
    }
    
    /* Open panel brightness control */
    int level_pct = (int)((g_backlight_level * 100) / 255);
    if (level_pct < 0) level_pct = 0;
    if (level_pct > 100) level_pct = 100;

    s_dimmer_is_panel = true;
    s_dimmer_tile_id = 0; /* dummy, unused in panel mode */
    lv_slider_set_range(s_dimmer_slider, 0, 100);
    lv_slider_set_value(s_dimmer_slider, level_pct, LV_ANIM_OFF);

    lv_label_set_text(s_dimmer_title, "Panel Brightness");
    char value_text[16];
    snprintf(value_text, sizeof(value_text), "%d%%", level_pct);
    lv_label_set_text(s_dimmer_value, value_text);

    dimmer_touch_activity();
    lv_obj_clear_flag(s_dimmer_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void global_touch_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING || code == LV_EVENT_CLICKED) {
        screen_wake();
    }
}

/* ============================================================
   PUBLIC API
   ============================================================ */

lv_color_t ui_panel_get_on_color(void)
{
    if (s_night_mode)
        return lv_color_make(COLOR_TILE_ON_NIGHT);
    return lv_color_make(COLOR_TILE_ON);
}

void ui_panel_set_night_mode(bool night_on)
{
    s_night_mode = night_on;

    /*
     * Repaint all tiles that are currently ON so the colour change
     * is immediately visible without waiting for the next state update.
     */
    lv_color_t on_color = ui_panel_get_on_color();
    for (int i = 0; i < MAX_TILES; i++) {
        if (s_tiles[i].tile && s_tiles[i].state) {
            lv_obj_set_style_bg_color(s_tiles[i].tile, on_color, LV_PART_MAIN);
        }
    }
}

void ui_panel_show_dimmer(int tile_id, const char *name, uint8_t level)
{
    if (!s_dimmer_overlay || !s_dimmer_slider || !s_dimmer_title || !s_dimmer_value) return;
    if (tile_id < 0 || tile_id >= MAX_TILES) return;

    s_dimmer_is_panel = false;
    s_dimmer_tile_id = tile_id;
    lv_slider_set_range(s_dimmer_slider, 0, 100);
    lv_slider_set_value(s_dimmer_slider, level, LV_ANIM_OFF);

    char title[64];
    snprintf(title, sizeof(title), "%s  (Dimmer)", name ? name : "Tile");
    lv_label_set_text(s_dimmer_title, title);

    char value_text[16];
    snprintf(value_text, sizeof(value_text), "%u%%", level);
    lv_label_set_text(s_dimmer_value, value_text);

    dimmer_touch_activity();
    lv_obj_clear_flag(s_dimmer_overlay, LV_OBJ_FLAG_HIDDEN);
}

void ui_panel_hide_dimmer(void)
{
    if (!s_dimmer_overlay) return;
    lv_obj_add_flag(s_dimmer_overlay, LV_OBJ_FLAG_HIDDEN);
    s_dimmer_tile_id = -1;
    s_dimmer_is_panel = false;
}

void ui_panel_update_dimmer_badge(int tile_id, bool dimmable)
{
    if (tile_id < 0 || tile_id >= MAX_TILES) return;
    if (!s_tiles[tile_id].dimmer_badge) return;

    if (dimmable) {
        lv_obj_clear_flag(s_tiles[tile_id].dimmer_badge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_tiles[tile_id].dimmer_badge, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ============================================================
   UI BUILD
   ============================================================ */

void ui_panel_build(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Building panel UI...");

    lv_obj_t *scr = lv_screen_active();

    /* Global touch listener: any touch resets inactivity timer */
    lv_obj_add_event_cb(scr, global_touch_cb, LV_EVENT_ALL, NULL);

    /* Screen background */
    lv_obj_set_style_bg_color(scr, lv_color_make(COLOR_BG), LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Header bar ---- */
    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_set_size(hdr, LCD_H_RES, HEADER_HEIGHT_PX);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(hdr, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(hdr, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hdr, lv_color_make(COLOR_HEADER_BG), LV_PART_MAIN);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hdr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hdr, header_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *hdr_lbl = lv_label_create(hdr);
    lv_label_set_text(hdr_lbl, LV_SYMBOL_HOME "  Home Assistant");
    lv_obj_set_style_text_color(hdr_lbl, lv_color_make(COLOR_HEADER_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(hdr_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(hdr_lbl);

    /* ---- Grid container ---- */
    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, LCD_H_RES, LCD_V_RES - HEADER_HEIGHT_PX);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    /* 2-column, 3-row equal-weight grid */
    static lv_coord_t col_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };
    static lv_coord_t row_dsc[] = {
        LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
    };
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    /* ---- 6 tile cards ---- */
    for (int i = 0; i < MAX_TILES; i++) {
        int col = i % 2;
        int row = i / 2;

        lv_obj_t *tile = lv_obj_create(grid);
        lv_obj_set_grid_cell(tile,
            LV_GRID_ALIGN_STRETCH, col, 1,
            LV_GRID_ALIGN_STRETCH, row, 1);

        lv_obj_set_style_radius(tile, TILE_CORNER_RADIUS, LV_PART_MAIN);
        lv_obj_set_style_border_width(tile, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(tile, lv_color_make(COLOR_TILE_EMPTY), LV_PART_MAIN);
        lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);

        /* Icon — centred slightly above mid */
        lv_obj_t *icon = lv_label_create(tile);
        lv_label_set_text(icon, LV_SYMBOL_POWER);
        lv_obj_set_style_text_font(icon, LV_FONT_DEFAULT, LV_PART_MAIN);
        lv_obj_set_style_text_color(icon, lv_color_make(COLOR_TILE_UNCFG_TEXT), LV_PART_MAIN);
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, TILE_ICON_OFFSET_Y);

        /* Name — single-line, centered below icon (truncate long names) */
        lv_obj_t *name = lv_label_create(tile);
        lv_label_set_text(name, "Empty");
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(name, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name, (LCD_H_RES / 2) - 20);
        lv_obj_align(name, LV_ALIGN_CENTER, 0, TILE_NAME_OFFSET_Y);

        /* State label — hidden (avoid overlaying names) */
        lv_obj_t *state = lv_label_create(tile);
        lv_label_set_text(state, "");
        lv_obj_set_style_text_font(state, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(state, lv_color_make(COLOR_STATE_LABEL), LV_PART_MAIN);
        lv_obj_add_flag(state, LV_OBJ_FLAG_HIDDEN);

        /* Dimmer badge — small indicator at top-right when dimmable */
        lv_obj_t *badge = lv_obj_create(tile);
        lv_obj_set_size(badge, 10, 10);
        lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -4, 4);
        lv_obj_set_style_radius(badge, 6, LV_PART_MAIN);        /* Circular */
        lv_obj_set_style_border_width(badge, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(badge, lv_color_make(0xFF, 0xA5, 0x00), LV_PART_MAIN); /* Orange */
        lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);             /* Hidden by default */
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);

        /* Populate tile struct */
        s_tiles[i].id          = i;
        s_tiles[i].tile        = tile;
        s_tiles[i].icon_label  = icon;
        s_tiles[i].name_label  = name;
        s_tiles[i].state_label = state;
        s_tiles[i].dimmer_badge = badge;
        s_tiles[i].state       = false;
        s_tiles[i].configured  = false;
        snprintf(s_tiles[i].name, TILE_NAME_MAX_LEN, "Tile_%d", i);

        /* Sync badge visibility with current tile state (if any) */
        ui_panel_update_dimmer_badge(i, s_tiles[i].dimmable);

        lv_obj_add_event_cb(tile, tile_event_cb, LV_EVENT_ALL, &s_tiles[i]);

        ESP_LOGD(TAG, "Tile[%d] created (row=%d col=%d)", i, row, col);
    }

    /* ---- Dimmer overlay (hidden by default, opened by tile long-press) ---- */
    s_dimmer_overlay = lv_obj_create(scr);
    lv_obj_set_size(s_dimmer_overlay, LCD_H_RES, LCD_V_RES);
    lv_obj_align(s_dimmer_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_dimmer_overlay, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_dimmer_overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_dimmer_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_dimmer_overlay, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_dimmer_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_dimmer_overlay, dimmer_overlay_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *card = lv_obj_create(s_dimmer_overlay);
    lv_obj_set_size(card, LCD_H_RES - 28, 180);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_make(0x2B, 0x2B, 0x2B), LV_PART_MAIN);

    s_dimmer_title = lv_label_create(card);
    lv_label_set_text(s_dimmer_title, "Dimmer");
    lv_obj_set_style_text_color(s_dimmer_title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_dimmer_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_dimmer_title, LV_ALIGN_TOP_MID, 0, 12);

    s_dimmer_slider = lv_slider_create(card);
    lv_obj_set_size(s_dimmer_slider, 26, 110);
    lv_obj_align(s_dimmer_slider, LV_ALIGN_CENTER, -40, 10);
    lv_slider_set_range(s_dimmer_slider, 0, 100);
    lv_slider_set_value(s_dimmer_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_dimmer_slider, dimmer_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Slider styling */
    lv_obj_set_style_radius(s_dimmer_slider, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_dimmer_slider, lv_color_make(0x3A, 0x3A, 0x3A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_dimmer_slider, lv_color_make(0xFF, 0xCC, 0x00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_dimmer_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_dimmer_slider, 2, LV_PART_MAIN);

    s_dimmer_value = lv_label_create(card);
    lv_label_set_text(s_dimmer_value, "0%");
    lv_obj_set_style_text_color(s_dimmer_value, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_dimmer_value, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(s_dimmer_value, LV_ALIGN_CENTER, 40, 12);

    ESP_LOGI(TAG, "Panel UI ready — %d tiles", MAX_TILES);
}

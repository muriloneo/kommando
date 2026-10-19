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
 * ui_panel.c — Pure LVGL UI layer.
 *
 * NO platform dependencies (no ESP32 SDK, no FreeRTOS, no Zigbee).
 * All interactions with the outside world go through the ui_platform_cbs_t
 * registered via ui_panel_init(). This makes the file compilable and
 * testable on any desktop host with an LVGL SDL2 backend (see simulator/).
 */

#include "ui_panel.h"
#include "ui_config.h"    /* dimensions, colors, timing constants     */
#include "ui_events.h"    /* ui_platform_cbs_t                        */
#include "ui_state.h"     /* s_tiles[], ui_tile_t                     */
#include "lvgl.h"
#include <stdio.h>        /* snprintf */

static const char *TAG = "ui_panel";

/* ============================================================
   PLATFORM CALLBACKS
   ============================================================ */

static const ui_platform_cbs_t *s_cbs = NULL;

void ui_panel_init(const ui_platform_cbs_t *cbs)
{
    s_cbs = cbs;

    /* LVGL 9 removed per-object long-press timing. Apply globally to all
     * registered input devices so every tile/dimmer trigger uses
     * TILE_LONG_PRESS_MS (dimming modal long-press behavior). */
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev != NULL) {
        lv_indev_set_long_press_time(indev, TILE_LONG_PRESS_MS);
        indev = lv_indev_get_next(indev);
    }
}

static inline bool screen_is_active(void)
{
    return s_cbs && s_cbs->is_screen_active ? s_cbs->is_screen_active() : true;
}

static inline bool screen_is_hw_sleeping(void)
{
    return s_cbs && s_cbs->is_screen_hw_sleeping ? s_cbs->is_screen_hw_sleeping() : false;
}

static inline void notify_screen_touch(void)
{
    if (s_cbs && s_cbs->on_screen_touch) s_cbs->on_screen_touch();
}

/* ============================================================
   WIDGET HANDLES
   ============================================================ */

static lv_obj_t *s_dimmer_overlay  = NULL;
static lv_obj_t *s_dimmer_title    = NULL;
static lv_obj_t *s_dimmer_value    = NULL;
static lv_obj_t *s_dimmer_slider   = NULL;
static lv_obj_t *s_conn_overlay    = NULL;
static lv_obj_t *s_conn_label      = NULL;
static lv_obj_t *s_conn_spinner    = NULL;
static int       s_dimmer_tile_id  = -1;
static bool      s_dimmer_is_panel = false;

static bool s_night_mode = false;

/* ============================================================
   DIMMER IDLE TIMER  (pure LVGL — no FreeRTOS needed)
   ============================================================ */

static lv_timer_t *s_dimmer_idle_timer    = NULL;
static uint32_t    s_dimmer_last_activity = 0;

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
    notify_screen_touch();
    s_dimmer_last_activity = lv_tick_get();
    if (!s_dimmer_idle_timer) {
        s_dimmer_idle_timer = lv_timer_create(dimmer_idle_timer_cb, 500, NULL);
    }
}

/* ============================================================
   BRIGHTNESS REPORT DEBOUNCE TIMER  (LVGL replaces FreeRTOS timer)
   ============================================================ */

static lv_timer_t *s_bl_report_timer = NULL;
static uint8_t     s_bl_pending_pct  = 100;

static void bl_report_timer_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    s_bl_report_timer = NULL;
    if (s_cbs && s_cbs->on_brightness_report) {
        s_cbs->on_brightness_report(s_bl_pending_pct);
    }
}

/* ============================================================
   TILE EVENT CALLBACK  (moved from tiles.c — pure LVGL + callbacks)
   ============================================================ */

static void tile_event_cb(lv_event_t *e)
{
    ui_tile_t *t = (ui_tile_t *)lv_event_get_user_data(e);
    if (!t) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_LONG_PRESSED) return;

    /* Capture screen state BEFORE waking — the wake call may change it */
    bool was_active      = screen_is_active();
    bool was_hw_sleeping = screen_is_hw_sleeping();

    notify_screen_touch();

    /*
     * Wake behaviour:
     * - From DIMMED:       first touch only wakes; does NOT toggle.
     * - From SCREEN_SLEEP: allow the touch to both wake AND toggle.
     */
    if (!was_active && !was_hw_sleeping && code != LV_EVENT_LONG_PRESSED) {
        UI_LOGI(TAG, "Wake tap — tile action suppressed");
        return;
    }

    if (code == LV_EVENT_LONG_PRESSED) {
        if (!t->dimmable) {
            UI_LOGI(TAG, "Tile[%d] hold ignored: not dimmable", t->id);
            return;
        }
        t->suppress_next_click = true;
        ui_panel_show_dimmer(t->id, t->name, t->level);
        if (s_cbs && s_cbs->on_tile_hold) s_cbs->on_tile_hold(t->id, t->level);
        UI_LOGI(TAG, "Tile[%d] hold → open dimmer (level=%u)", t->id, t->level);
        return;
    }

    /* Click path */
    if (t->suppress_next_click) {
        t->suppress_next_click = false;
        UI_LOGD(TAG, "Tile[%d] click suppressed after long press", t->id);
        return;
    }

    bool old_state = t->state;
    t->state = !t->state;

    if (t->tile) {
        lv_obj_set_style_bg_color(t->tile,
            t->state ? ui_panel_get_on_color() : lv_color_make(COLOR_TILE_OFF),
            LV_PART_MAIN);
    }

    UI_LOGI(TAG, "Tile[%d] '%s' %s → %s",
             t->id,
             t->configured ? t->name : "(unconfigured)",
             old_state ? "ON" : "OFF",
             t->state  ? "ON" : "OFF");

    if (s_cbs && s_cbs->on_tile_tap) s_cbs->on_tile_tap(t->id, t->state);
}

/* ============================================================
   OTHER EVENT CALLBACKS
   ============================================================ */

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
        /* Apply hardware brightness immediately for smooth feedback */
        s_bl_pending_pct = (uint8_t)value;
        if (s_cbs && s_cbs->on_brightness_apply) {
            s_cbs->on_brightness_apply(s_bl_pending_pct);
        }

        /* Debounce network report with a 150 ms LVGL timer */
        if (s_bl_report_timer) {
            lv_timer_reset(s_bl_report_timer);
        } else {
            s_bl_report_timer = lv_timer_create(bl_report_timer_cb, 150, NULL);
            lv_timer_set_repeat_count(s_bl_report_timer, 1);
        }
    } else {
        /* Tile dimmer — forward to platform immediately */
        if (s_cbs && s_cbs->on_dim_change) {
            s_cbs->on_dim_change(s_dimmer_tile_id, (uint8_t)value);
        }
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

    bool was_active = screen_is_active();
    notify_screen_touch();

    /* If waking from sleep/dim, suppress the brightness panel open */
    if (!was_active) return;

    /* Query current brightness via platform callback (0–100%) */
    int level_pct = s_cbs && s_cbs->get_backlight_pct
                    ? s_cbs->get_backlight_pct()
                    : 100;
    if (level_pct < 0)   level_pct = 0;
    if (level_pct > 100) level_pct = 100;

    s_dimmer_is_panel = true;
    s_dimmer_tile_id  = 0;   /* unused in panel mode */
    lv_slider_set_range(s_dimmer_slider, 0, 100);
    lv_slider_set_value(s_dimmer_slider, level_pct, LV_ANIM_OFF);

    lv_label_set_text(s_dimmer_title, "Kommando Brightness");
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
        notify_screen_touch();
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

void ui_panel_apply_tile(int tile_id, bool configured, const char *name,
                         bool dimmable, bool state, uint8_t level)
{
    if (tile_id < 0 || tile_id >= MAX_TILES) return;
    ui_tile_t *t = &s_tiles[tile_id];

    /* --- Update logical state --- */
    t->configured = configured;
    t->dimmable   = dimmable;
    t->state      = state;
    t->level      = level;
    if (name && configured) {
        strncpy(t->name, name, TILE_NAME_MAX_LEN - 1);
        t->name[TILE_NAME_MAX_LEN - 1] = '\0';
    }

    /* --- Paint LVGL objects (only if ui_panel_build() has run) --- */
    if (!t->tile) return;

    lv_color_t bg;
    if (!configured) {
        bg = lv_color_make(COLOR_TILE_EMPTY);
    } else if (state) {
        bg = ui_panel_get_on_color();
    } else {
        bg = lv_color_make(COLOR_TILE_OFF);
    }
    lv_obj_set_style_bg_color(t->tile, bg, LV_PART_MAIN);

    if (t->name_label) {
        lv_label_set_text(t->name_label, (configured && name) ? name : "Empty");
    }

    ui_panel_update_dimmer_badge(tile_id, dimmable);
}

void ui_panel_set_night_mode(bool night_on)
{
    s_night_mode = night_on;

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

void ui_panel_set_connection_overlay(bool connected, const char *status_text)
{
    if (!s_conn_overlay) return;

    if (connected) {
        lv_obj_add_flag(s_conn_overlay, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (status_text && s_conn_label) {
        lv_label_set_text(s_conn_label, status_text);
    }
    lv_obj_clear_flag(s_conn_overlay, LV_OBJ_FLAG_HIDDEN);
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
    (void)disp;   /* Reserved for multi-display use */

    /* Initialise theme styles on first build */
    ui_theme_init();

    UI_LOGI(TAG, "Building panel UI...");

    lv_obj_t *scr = lv_screen_active();

    /* Global touch listener: any touch resets inactivity timer */
    lv_obj_add_event_cb(scr, global_touch_cb, LV_EVENT_ALL, NULL);

    /* Screen background */
    ui_theme_apply_screen(scr);

    /* ---- Header bar ---- */
    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_t *hdr_lbl = lv_label_create(hdr);
    lv_label_set_text(hdr_lbl, LV_SYMBOL_HOME "  Kommando");
    lv_obj_add_event_cb(hdr, header_click_cb, LV_EVENT_CLICKED, NULL);
    ui_theme_apply_header(hdr, hdr_lbl);

    /* ---- Grid container ---- */
    lv_obj_t *grid = lv_obj_create(scr);
    ui_theme_apply_grid(grid);

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

        ui_theme_apply_tile_base(tile);

        /* Icon — centred slightly above mid */
        lv_obj_t *icon = lv_label_create(tile);
        lv_label_set_text(icon, LV_SYMBOL_POWER);

        /* Name — single-line, centred below icon, truncated on overflow */
        lv_obj_t *name = lv_label_create(tile);
        lv_label_set_text(name, "Empty");

        /* State label — hidden by default */
        lv_obj_t *state = lv_label_create(tile);
        lv_label_set_text(state, "");

        /* Dimmer badge — small orange dot at top-right */
        lv_obj_t *badge = lv_obj_create(tile);

        ui_theme_apply_tile_children(icon, name, state, badge);

        /* Populate tile state */
        s_tiles[i].id           = i;
        s_tiles[i].tile         = tile;
        s_tiles[i].icon_label   = icon;
        s_tiles[i].name_label   = name;
        s_tiles[i].state_label  = state;
        s_tiles[i].dimmer_badge = badge;
        s_tiles[i].state        = false;
        s_tiles[i].configured   = false;
        snprintf(s_tiles[i].name, TILE_NAME_MAX_LEN, "Tile_%d", i);

        /* Sync badge with any pre-existing dimmable state (e.g. NVS reload) */
        ui_panel_update_dimmer_badge(i, s_tiles[i].dimmable);

        lv_obj_add_event_cb(tile, tile_event_cb, LV_EVENT_ALL, &s_tiles[i]);

        UI_LOGD(TAG, "Tile[%d] created (row=%d col=%d)", i, row, col);
    }

    /* ---- Connection overlay (shown until Zigbee joins) ----------------- */
    s_conn_overlay = lv_obj_create(scr);
    s_conn_spinner = lv_spinner_create(s_conn_overlay);
    lv_obj_set_size(s_conn_spinner, UI_CONN_SPINNER_SIZE, UI_CONN_SPINNER_SIZE);
    lv_obj_align(s_conn_spinner, LV_ALIGN_CENTER, 0, -20);

    s_conn_label = lv_label_create(s_conn_overlay);
    lv_label_set_text(s_conn_label, "Connecting to Zigbee...");

    ui_theme_apply_conn_overlay(s_conn_overlay, s_conn_label);

    /* ---- Dimmer overlay (hidden; opened by long-press) ----------------- */
    s_dimmer_overlay = lv_obj_create(scr);
    ui_theme_apply_dimmer_overlay(s_dimmer_overlay);
    lv_obj_add_event_cb(s_dimmer_overlay, dimmer_overlay_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *card = lv_obj_create(s_dimmer_overlay);

    s_dimmer_title = lv_label_create(card);
    lv_label_set_text(s_dimmer_title, "Dimmer");

    s_dimmer_slider = lv_slider_create(card);
    lv_slider_set_range(s_dimmer_slider, 0, 100);
    lv_slider_set_value(s_dimmer_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_dimmer_slider, dimmer_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_dimmer_value = lv_label_create(card);
    lv_label_set_text(s_dimmer_value, "0%");

    ui_theme_apply_dimmer_card(card, s_dimmer_title, s_dimmer_slider, s_dimmer_value);

    UI_LOGI(TAG, "Panel UI ready — %d tiles", MAX_TILES);
}

/*
 * Copyright (C) 2026 Murilo Freire
 *
 * This file is part of Kommando firmware.
 *
 * Kommando firmware is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ui_theme.c — LVGL style preset definitions for the Kommando UI.
 *
 * No platform dependencies — compiles for both firmware and simulator.
 */
#include "ui_theme.h"
#include "lvgl.h"

/* ============================================================
   STATIC STYLE OBJECTS
   ============================================================ */

lv_style_t theme_style_screen;
lv_style_t theme_style_header;
lv_style_t theme_style_header_label;
lv_style_t theme_style_grid;
lv_style_t theme_style_tile_base;
lv_style_t theme_style_tile_icon;
lv_style_t theme_style_tile_name;
lv_style_t theme_style_tile_state;
lv_style_t theme_style_dimmer_badge;
lv_style_t theme_style_conn_overlay;
lv_style_t theme_style_conn_label;
lv_style_t theme_style_dimmer_overlay;
lv_style_t theme_style_dimmer_card;
lv_style_t theme_style_dimmer_title;
lv_style_t theme_style_dimmer_slider_main;
lv_style_t theme_style_dimmer_slider_indicator;
lv_style_t theme_style_dimmer_slider_knob;
lv_style_t theme_style_dimmer_value;

/* Internal flag to make init idempotent */
static bool s_theme_initialised = false;

/* ============================================================
   HELPERS
   ============================================================ */

static inline void style_init_if_needed(lv_style_t *style)
{
    if (!s_theme_initialised) {
        lv_style_init(style);
    } else {
        /* Re-initialise to allow theme re-application (e.g. after
         * display re-init).  lv_style_reset() is available in LVGL 9. */
        lv_style_reset(style);
    }
}

/* ============================================================
   PUBLIC API
   ============================================================ */

void ui_theme_init(void)
{
    if (s_theme_initialised) return;

    /* ---- Screen / background ---- */
    style_init_if_needed(&theme_style_screen);
    lv_style_set_bg_color(&theme_style_screen, lv_color_make(COLOR_BG));
    lv_style_set_bg_opa(&theme_style_screen, LV_OPA_COVER);
    lv_style_set_radius(&theme_style_screen, 0);
    lv_style_set_border_width(&theme_style_screen, 0);
    lv_style_set_pad_all(&theme_style_screen, 0);

    /* ---- Header bar ---- */
    style_init_if_needed(&theme_style_header);
    lv_style_set_bg_color(&theme_style_header, lv_color_make(COLOR_HEADER_BG));
    lv_style_set_bg_opa(&theme_style_header, LV_OPA_COVER);
    lv_style_set_radius(&theme_style_header, 0);
    lv_style_set_border_width(&theme_style_header, 0);
    lv_style_set_pad_all(&theme_style_header, 0);
    lv_style_set_width(&theme_style_header, LCD_H_RES);
    lv_style_set_height(&theme_style_header, HEADER_HEIGHT_PX);

    style_init_if_needed(&theme_style_header_label);
    lv_style_set_text_color(&theme_style_header_label, lv_color_make(COLOR_HEADER_TEXT));
    lv_style_set_text_font(&theme_style_header_label, &lv_font_montserrat_14);
    lv_style_set_text_align(&theme_style_header_label, LV_TEXT_ALIGN_CENTER);

    /* ---- Grid container ---- */
    style_init_if_needed(&theme_style_grid);
    lv_style_set_bg_opa(&theme_style_grid, LV_OPA_TRANSP);
    lv_style_set_border_width(&theme_style_grid, 0);
    lv_style_set_pad_all(&theme_style_grid, 0);
    lv_style_set_radius(&theme_style_grid, 0);
    lv_style_set_width(&theme_style_grid, LCD_H_RES);
    lv_style_set_height(&theme_style_grid, LCD_V_RES - HEADER_HEIGHT_PX);

    /* ---- Tile base card ---- */
    style_init_if_needed(&theme_style_tile_base);
    lv_style_set_radius(&theme_style_tile_base, TILE_CORNER_RADIUS);
    lv_style_set_border_width(&theme_style_tile_base, 0);
    lv_style_set_bg_color(&theme_style_tile_base, lv_color_make(COLOR_TILE_EMPTY));
    lv_style_set_bg_opa(&theme_style_tile_base, LV_OPA_COVER);
    lv_style_set_width(&theme_style_tile_base, LV_PCT(100));
    lv_style_set_height(&theme_style_tile_base, LV_PCT(100));

    /* ---- Tile icon ---- */
    style_init_if_needed(&theme_style_tile_icon);
    lv_style_set_text_font(&theme_style_tile_icon, LV_FONT_DEFAULT);
    lv_style_set_text_color(&theme_style_tile_icon, lv_color_make(COLOR_TILE_UNCFG_TEXT));
    lv_style_set_text_align(&theme_style_tile_icon, LV_TEXT_ALIGN_CENTER);
    /* Position handled by lv_obj_align() in ui_panel_build */

    /* ---- Tile name label ---- */
    style_init_if_needed(&theme_style_tile_name);
    lv_style_set_text_font(&theme_style_tile_name, &lv_font_montserrat_14);
    lv_style_set_text_color(&theme_style_tile_name, lv_color_white());
    lv_style_set_text_align(&theme_style_tile_name, LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_letter_space(&theme_style_tile_name, 0);
    lv_style_set_text_line_space(&theme_style_tile_name, 0);
    lv_style_set_width(&theme_style_tile_name, (LCD_H_RES / TILE_GRID_COLS) - 20);
    /* Long mode handled in ui_panel_build */

    /* ---- Tile state label (ON/OFF) ---- */
    style_init_if_needed(&theme_style_tile_state);
    lv_style_set_text_font(&theme_style_tile_state, &lv_font_montserrat_14);
    lv_style_set_text_color(&theme_style_tile_state, lv_color_make(COLOR_STATE_LABEL));
    lv_style_set_text_align(&theme_style_tile_state, LV_TEXT_ALIGN_CENTER);
    lv_style_set_opa(&theme_style_tile_state, LV_OPA_TRANSP);  /* Hidden by default */

    /* ---- Dimmer badge (orange dot) ---- */
    style_init_if_needed(&theme_style_dimmer_badge);
    lv_style_set_radius(&theme_style_dimmer_badge, UI_BADGE_RADIUS);
    lv_style_set_border_width(&theme_style_dimmer_badge, 0);
    lv_style_set_bg_color(&theme_style_dimmer_badge, lv_color_make(COLOR_BADGE_DIMMABLE));
    lv_style_set_bg_opa(&theme_style_dimmer_badge, LV_OPA_COVER);
    lv_style_set_width(&theme_style_dimmer_badge, UI_BADGE_SIZE);
    lv_style_set_height(&theme_style_dimmer_badge, UI_BADGE_SIZE);
    lv_style_set_opa(&theme_style_dimmer_badge, LV_OPA_TRANSP);  /* Hidden by default */

    /* ---- Connection overlay ---- */
    style_init_if_needed(&theme_style_conn_overlay);
    lv_style_set_bg_color(&theme_style_conn_overlay, lv_color_black());
    lv_style_set_bg_opa(&theme_style_conn_overlay, UI_OVERLAY_OPA_CONN);
    lv_style_set_radius(&theme_style_conn_overlay, 0);
    lv_style_set_border_width(&theme_style_conn_overlay, 0);
    lv_style_set_pad_all(&theme_style_conn_overlay, 0);
    lv_style_set_width(&theme_style_conn_overlay, LCD_H_RES);
    lv_style_set_height(&theme_style_conn_overlay, LCD_V_RES);

    style_init_if_needed(&theme_style_conn_label);
    lv_style_set_text_color(&theme_style_conn_label, lv_color_white());
    lv_style_set_text_font(&theme_style_conn_label, &lv_font_montserrat_14);
    lv_style_set_text_align(&theme_style_conn_label, LV_TEXT_ALIGN_CENTER);

    /* ---- Dimmer overlay ---- */
    style_init_if_needed(&theme_style_dimmer_overlay);
    lv_style_set_bg_color(&theme_style_dimmer_overlay, lv_color_black());
    lv_style_set_bg_opa(&theme_style_dimmer_overlay, UI_OVERLAY_OPA_DIM);
    lv_style_set_radius(&theme_style_dimmer_overlay, 0);
    lv_style_set_border_width(&theme_style_dimmer_overlay, 0);
    lv_style_set_pad_all(&theme_style_dimmer_overlay, 0);
    lv_style_set_width(&theme_style_dimmer_overlay, LCD_H_RES);
    lv_style_set_height(&theme_style_dimmer_overlay, LCD_V_RES);

    /* ---- Dimmer card ---- */
    style_init_if_needed(&theme_style_dimmer_card);
    lv_style_set_bg_color(&theme_style_dimmer_card, lv_color_make(COLOR_DIMMER_CARD_BG));
    lv_style_set_bg_opa(&theme_style_dimmer_card, LV_OPA_COVER);
    lv_style_set_radius(&theme_style_dimmer_card, UI_DIMMER_CARD_RADIUS);
    lv_style_set_border_width(&theme_style_dimmer_card, 0);
    lv_style_set_width(&theme_style_dimmer_card, UI_DIMMER_CARD_W);
    lv_style_set_height(&theme_style_dimmer_card, UI_DIMMER_CARD_H);

    /* ---- Dimmer title label ---- */
    style_init_if_needed(&theme_style_dimmer_title);
    lv_style_set_text_color(&theme_style_dimmer_title, lv_color_white());
    lv_style_set_text_font(&theme_style_dimmer_title, &lv_font_montserrat_14);
    lv_style_set_text_align(&theme_style_dimmer_title, LV_TEXT_ALIGN_CENTER);

    /* ---- Dimmer slider parts ---- */
    style_init_if_needed(&theme_style_dimmer_slider_main);
    lv_style_set_radius(&theme_style_dimmer_slider_main, 8);
    lv_style_set_bg_color(&theme_style_dimmer_slider_main, lv_color_make(COLOR_SLIDER_TRACK));
    lv_style_set_pad_all(&theme_style_dimmer_slider_main, 2);
    lv_style_set_width(&theme_style_dimmer_slider_main, UI_SLIDER_W);
    lv_style_set_height(&theme_style_dimmer_slider_main, UI_SLIDER_H);

    style_init_if_needed(&theme_style_dimmer_slider_indicator);
    lv_style_set_bg_color(&theme_style_dimmer_slider_indicator, lv_color_make(COLOR_TILE_ON));
    lv_style_set_radius(&theme_style_dimmer_slider_indicator, 8);

    style_init_if_needed(&theme_style_dimmer_slider_knob);
    lv_style_set_bg_color(&theme_style_dimmer_slider_knob, lv_color_white());
    lv_style_set_radius(&theme_style_dimmer_slider_knob, LV_RADIUS_CIRCLE);

    /* ---- Dimmer value label ---- */
    style_init_if_needed(&theme_style_dimmer_value);
    lv_style_set_text_color(&theme_style_dimmer_value, lv_color_white());
    lv_style_set_text_font(&theme_style_dimmer_value, &lv_font_montserrat_24);
    lv_style_set_text_align(&theme_style_dimmer_value, LV_TEXT_ALIGN_CENTER);

    s_theme_initialised = true;
}

void ui_theme_apply_screen(lv_obj_t *scr)
{
    lv_obj_add_style(scr, &theme_style_screen, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_theme_apply_header(lv_obj_t *hdr, lv_obj_t *hdr_lbl)
{
    lv_obj_add_style(hdr, &theme_style_header, LV_PART_MAIN);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hdr, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_style(hdr_lbl, &theme_style_header_label, LV_PART_MAIN);
    lv_obj_center(hdr_lbl);
}

void ui_theme_apply_grid(lv_obj_t *grid)
{
    lv_obj_add_style(grid, &theme_style_grid, LV_PART_MAIN);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    /* Grid layout descriptors are set in ui_panel_build() */
}

void ui_theme_apply_tile_base(lv_obj_t *tile)
{
    lv_obj_add_style(tile, &theme_style_tile_base, LV_PART_MAIN);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    /* Grid cell placement is set in ui_panel_build() */
}

void ui_theme_apply_tile_children(
    lv_obj_t *icon,
    lv_obj_t *name,
    lv_obj_t *state,
    lv_obj_t *badge)
{
    lv_obj_add_style(icon, &theme_style_tile_icon, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, TILE_ICON_OFFSET_Y);

    lv_obj_add_style(name, &theme_style_tile_name, LV_PART_MAIN);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, TILE_NAME_OFFSET_Y);

    lv_obj_add_style(state, &theme_style_tile_state, LV_PART_MAIN);
    lv_obj_add_flag(state, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_style(badge, &theme_style_dimmer_badge, LV_PART_MAIN);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
}

void ui_theme_apply_conn_overlay(lv_obj_t *overlay, lv_obj_t *label)
{
    lv_obj_add_style(overlay, &theme_style_conn_overlay, LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_style(label, &theme_style_conn_label, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 22);
}

void ui_theme_apply_dimmer_overlay(lv_obj_t *overlay)
{
    lv_obj_add_style(overlay, &theme_style_dimmer_overlay, LV_PART_MAIN);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
}

void ui_theme_apply_dimmer_card(
    lv_obj_t *card,
    lv_obj_t *title,
    lv_obj_t *slider,
    lv_obj_t *value)
{
    lv_obj_add_style(card, &theme_style_dimmer_card, LV_PART_MAIN);
    lv_obj_center(card);

    lv_obj_add_style(title, &theme_style_dimmer_title, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_add_style(slider, &theme_style_dimmer_slider_main, LV_PART_MAIN);
    lv_obj_add_style(slider, &theme_style_dimmer_slider_indicator, LV_PART_INDICATOR);
    lv_obj_add_style(slider, &theme_style_dimmer_slider_knob, LV_PART_KNOB);
    lv_obj_align(slider, LV_ALIGN_CENTER, -40, 10);

    lv_obj_add_style(value, &theme_style_dimmer_value, LV_PART_MAIN);
    lv_obj_align(value, LV_ALIGN_CENTER, 40, 12);
}
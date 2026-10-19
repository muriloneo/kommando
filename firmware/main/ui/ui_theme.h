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
 * ui_theme.h — Static LVGL style presets for the Kommando UI.
 *
 * All hard-coded lv_obj_set_style_* calls from ui_panel.c are centralized
 * here as pre-initialised lv_style_t objects.  ui_panel_build() applies
 * these styles with lv_obj_add_style() instead of inline calls.
 *
 * No platform dependencies — compiles for both firmware and simulator.
 */
#pragma once

#include "lvgl.h"
#include "ui_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
   STYLE DECLARATIONS
   ============================================================ */

/* ---- Screen / background ---- */
extern lv_style_t theme_style_screen;

/* ---- Header bar ---- */
extern lv_style_t theme_style_header;
extern lv_style_t theme_style_header_label;

/* ---- Grid container ---- */
extern lv_style_t theme_style_grid;

/* ---- Tile card (base, unconfigured/off/on set via colour override) ---- */
extern lv_style_t theme_style_tile_base;

/* ---- Tile icon ---- */
extern lv_style_t theme_style_tile_icon;

/* ---- Tile name label ---- */
extern lv_style_t theme_style_tile_name;

/* ---- Tile state label (ON/OFF) ---- */
extern lv_style_t theme_style_tile_state;

/* ---- Dimmer badge (orange dot) ---- */
extern lv_style_t theme_style_dimmer_badge;

/* ---- Connection overlay ---- */
extern lv_style_t theme_style_conn_overlay;
extern lv_style_t theme_style_conn_label;

/* ---- Dimmer overlay ---- */
extern lv_style_t theme_style_dimmer_overlay;

/* ---- Dimmer card ---- */
extern lv_style_t theme_style_dimmer_card;

/* ---- Dimmer title label ---- */
extern lv_style_t theme_style_dimmer_title;

/* ---- Dimmer slider (track / indicator / knob) ---- */
extern lv_style_t theme_style_dimmer_slider_main;
extern lv_style_t theme_style_dimmer_slider_indicator;
extern lv_style_t theme_style_dimmer_slider_knob;

/* ---- Dimmer value label ---- */
extern lv_style_t theme_style_dimmer_value;

/* ============================================================
   INITIALISATION
   ============================================================ */

/**
 * Initialise all theme styles.
 * Must be called once after lv_init() and before ui_panel_build().
 * Safe to call multiple times (idempotent).
 */
void ui_theme_init(void);

/**
 * Apply the screen background style to the active screen.
 */
void ui_theme_apply_screen(lv_obj_t *scr);

/**
 * Apply the header bar style to a header object.
 */
void ui_theme_apply_header(lv_obj_t *hdr, lv_obj_t *hdr_lbl);

/**
 * Apply the grid container style.
 */
void ui_theme_apply_grid(lv_obj_t *grid);

/**
 * Apply the base tile card style to a tile object.
 * Caller should override bg colour per tile state.
 */
void ui_theme_apply_tile_base(lv_obj_t *tile);

/**
 * Apply styles to tile child widgets (icon, name, state, badge).
 * Called once per tile during ui_panel_build().
 */
void ui_theme_apply_tile_children(
    lv_obj_t *icon,
    lv_obj_t *name,
    lv_obj_t *state,
    lv_obj_t *badge);

/**
 * Apply connection overlay styles.
 */
void ui_theme_apply_conn_overlay(lv_obj_t *overlay, lv_obj_t *label);

/**
 * Apply dimmer overlay styles.
 */
void ui_theme_apply_dimmer_overlay(lv_obj_t *overlay);

/**
 * Apply dimmer card and its children styles.
 */
void ui_theme_apply_dimmer_card(
    lv_obj_t *card,
    lv_obj_t *title,
    lv_obj_t *slider,
    lv_obj_t *value);

#ifdef __cplusplus
}
#endif
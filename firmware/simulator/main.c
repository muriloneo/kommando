/**
 * simulator/main.c — Kommando UI desktop simulator.
 *
 * Builds the LVGL UI against an SDL2 window so you can iterate on
 * ui_panel.c / ui_state.c without touching any ESP32 hardware.
 *
 * Usage (after cmake build):
 *   ./kommando_sim
 *
 * Keyboard shortcuts (handled in the event loop):
 *   N  — toggle night mode
 *   C  — cycle connection overlay (connecting / connected)
 *   0–5 — toggle tile 0–5 on/off
 *   +/- — increase / decrease backlight (logged only)
 *   Q / Esc — quit
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "SDL2/SDL.h"
#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"

#include "ui/ui_panel.h"
#include "ui/ui_state.h"
#include "ui/ui_config.h"

/* ============================================================
   SIMULATOR STATE
   ============================================================ */

static bool  s_screen_active    = true;
static bool  s_hw_sleeping      = false;
static int   s_backlight_pct    = 100;
static bool  s_night_mode       = false;
static bool  s_connected        = true;

/* ============================================================
   PLATFORM CALLBACK IMPLEMENTATIONS (mock)
   ============================================================ */

static bool sim_is_screen_active(void)      { return s_screen_active; }
static bool sim_is_hw_sleeping(void)        { return s_hw_sleeping; }

static void sim_on_screen_touch(void)
{
    if (!s_screen_active) {
        s_screen_active = true;
        printf("[sim] screen wake\n");
    }
}

static void sim_on_tile_tap(int tile_id, bool new_state)
{
    printf("[sim] tile[%d] tap → %s\n", tile_id, new_state ? "ON" : "OFF");
    s_tiles[tile_id].state = new_state;
}

static void sim_on_tile_hold(int tile_id, uint8_t current_level)
{
    printf("[sim] tile[%d] hold (level=%u%%) → open dimmer\n", tile_id, current_level);
    ui_panel_show_dimmer(tile_id, s_tiles[tile_id].name, current_level);
}

static void sim_on_dim_change(int tile_id, uint8_t level_pct)
{
    printf("[sim] tile[%d] dim → %u%%\n", tile_id, level_pct);
    ui_panel_apply_tile(tile_id, s_tiles[tile_id].configured, s_tiles[tile_id].name,
                        s_tiles[tile_id].dimmable, s_tiles[tile_id].state, level_pct);
}

static int sim_get_backlight_pct(void)      { return s_backlight_pct; }

static void sim_brightness_apply(uint8_t pct)
{
    s_backlight_pct = pct;
    printf("[sim] brightness apply → %u%%\n", pct);
}

static void sim_brightness_report(uint8_t pct)
{
    printf("[sim] brightness report (network) → %u%%\n", pct);
}

static const ui_platform_cbs_t s_sim_cbs = {
    .is_screen_active      = sim_is_screen_active,
    .is_screen_hw_sleeping = sim_is_hw_sleeping,
    .on_screen_touch       = sim_on_screen_touch,
    .on_tile_tap           = sim_on_tile_tap,
    .on_tile_hold          = sim_on_tile_hold,
    .on_dim_change         = sim_on_dim_change,
    .get_backlight_pct     = sim_get_backlight_pct,
    .on_brightness_apply   = sim_brightness_apply,
    .on_brightness_report  = sim_brightness_report,
};

/* ============================================================
   DEMO TILE CONFIGURATION
   Sets up a realistic 6-tile layout for UI testing.
   ============================================================ */

static void demo_configure_tiles(void)
{
    /* Tile 0: dimmable ceiling light */
    ui_panel_apply_tile(0, true,  "Ceiling",  true,  true,  75);

    /* Tile 1: simple on/off lamp */
    ui_panel_apply_tile(1, true,  "Lamp",     false, false, 0);

    /* Tile 2: dimmable strip */
    ui_panel_apply_tile(2, true,  "Strip",    true,  true,  40);

    /* Tile 3: unconfigured (shows placeholder) */
    ui_panel_apply_tile(3, false, "Tile_3",   false, false, 0);

    /* Tile 4: configured, currently off */
    ui_panel_apply_tile(4, true,  "Kitchen",  false, false, 0);

    /* Tile 5: dimmable, off */
    ui_panel_apply_tile(5, true,  "Hallway",  true,  false, 0);
}

/* ============================================================
   KEYBOARD HANDLER  (SDL_KEYDOWN events)
   ============================================================ */

static void handle_key(SDL_Keycode key)
{
    switch (key) {
        case SDLK_n:
            s_night_mode = !s_night_mode;
            ui_panel_set_night_mode(s_night_mode);
            printf("[sim] night mode → %s\n", s_night_mode ? "ON" : "OFF");
            break;

        case SDLK_c:
            s_connected = !s_connected;
            ui_panel_set_connection_overlay(s_connected, s_connected ? NULL : "Connecting…");
            printf("[sim] connection → %s\n", s_connected ? "connected" : "connecting");
            break;

        case SDLK_0: case SDLK_1: case SDLK_2:
        case SDLK_3: case SDLK_4: case SDLK_5: {
            int idx = key - SDLK_0;
            ui_panel_apply_tile(idx, s_tiles[idx].configured, s_tiles[idx].name,
                                s_tiles[idx].dimmable, !s_tiles[idx].state, s_tiles[idx].level);
            printf("[sim] keyboard toggle tile[%d] → %s\n", idx, s_tiles[idx].state ? "ON" : "OFF");
            break;
        }

        case SDLK_PLUS: case SDLK_EQUALS:
            s_backlight_pct = (s_backlight_pct < 100) ? s_backlight_pct + 10 : 100;
            printf("[sim] backlight → %d%%\n", s_backlight_pct);
            break;

        case SDLK_MINUS:
            s_backlight_pct = (s_backlight_pct > 0) ? s_backlight_pct - 10 : 0;
            printf("[sim] backlight → %d%%\n", s_backlight_pct);
            break;

        default:
            break;
    }
}

/* ============================================================
   MAIN
   ============================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    /* --- LVGL init --- */
    lv_init();

    /* --- SDL2 display (matches real panel resolution 172×320) --- */
    lv_display_t *disp = lv_sdl_window_create(LCD_H_RES, LCD_V_RES);
    lv_sdl_window_set_title(disp, "Kommando UI Simulator");
    /* 2× zoom so the narrow panel is readable on a desktop monitor */
    lv_sdl_window_set_zoom(disp, 2.0f);

    /* --- SDL2 mouse/touch input --- */
    lv_indev_t *mouse = lv_sdl_mouse_create();
    (void)mouse;

    /* --- Wire platform callbacks and build the UI --- */
    ui_panel_init(&s_sim_cbs);
    ui_panel_build(disp);

    /* --- Apply demo tile layout (after build, so it isn't clobbered) --- */
    demo_configure_tiles();

    /* Start in "connected" state (hide spinner) */
    ui_panel_set_connection_overlay(true, NULL);

    printf("[sim] Kommando UI Simulator running (%dx%d @ 2x zoom)\n", LCD_H_RES, LCD_V_RES);
    printf("[sim] Keys: N=night  C=connection  0-5=toggle tile  +/-=brightness  Q/Esc=quit\n");

    /* --- Main event loop --- */
    bool running = true;
    while (running) {
        /* Feed SDL events to LVGL and handle our hotkeys */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_q ||
                    e.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else {
                    handle_key(e.key.keysym.sym);
                }
            }
        }

        /* LVGL timer handler (runs animations, redraws, etc.) */
        uint32_t time_till_next = lv_timer_handler();
        SDL_Delay(time_till_next > 10 ? 10 : (uint32_t)time_till_next);
    }

    lv_sdl_quit();
    SDL_Quit();
    return 0;
}

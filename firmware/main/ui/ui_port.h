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
 * ui_port.h — Platform port abstraction for the Kommando UI.
 *
 * This header provides the platform callback interface that the
 * hardware-free UI layer (ui_panel.c) uses to interact with the
 * underlying system (ESP32 firmware or desktop simulator).
 *
 * It is a thin wrapper over ui_events.h for clearer naming:
 * "port" = the platform-specific port of the UI.
 *
 * Implementations:
 *   - ESP32 firmware: firmware/main/tiles.c provides tiles_on_tap(),
 *     tiles_on_hold(), tile_slider_level_cb(), screen_wake(), etc.
 *   - Desktop simulator: firmware/simulator/main.c provides mock
 *     implementations using printf/SDL stubs.
 */
#pragma once

#include "ui_events.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
   TYPE ALIASES (for clearer "port" nomenclature)
   ============================================================ */

/**
 * Platform callback table — the "port" that connects the UI to
 * the underlying system.
 * 
 * Use ui_port_cbs_t in new code; ui_platform_cbs_t is the
 * canonical type defined in ui_events.h.
 */
typedef ui_platform_cbs_t ui_port_cbs_t;

/**
 * Initialise the UI panel with a platform port implementation.
 * 
 * @param port  Pointer to a statically-allocated ui_port_cbs_t
 *              (same as ui_platform_cbs_t).  The struct must
 *              remain valid for the lifetime of the UI.
 * 
 * See ui_panel_init() in ui_panel.h for full documentation.
 */
static inline void ui_port_init(const ui_port_cbs_t *port)
{
    ui_panel_init(port);
}

#ifdef __cplusplus
}
#endif
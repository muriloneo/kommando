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
 * commands_parse.h — Pure-C parser for the HA command protocol.
 *
 * No ESP-IDF / LVGL / NVS dependencies. Host-testable with assert.h.
 *
 * Command grammar (mirrors blueprint contract):
 *   C:<id>:<icon>:<name>      Configure tile
 *   S:<id>:<0|1>              Set visual state
 *   D:<id>:<0|1>:<0..100>     Configure dimmer enable + level
 *   L:<id>:<0..100>           Update dimmer level only
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CMD_PARSE_MAX_TILES
#define CMD_PARSE_MAX_TILES        6
#endif
#ifndef CMD_PARSE_NAME_MAX
#define CMD_PARSE_NAME_MAX        32
#endif
#ifndef CMD_PARSE_ICON_MAX
#define CMD_PARSE_ICON_MAX        16
#endif
#ifndef CMD_PARSE_PAYLOAD_MAX
#define CMD_PARSE_PAYLOAD_MAX     64
#endif

typedef enum {
    CMD_PARSE_OK              = 0,
    CMD_PARSE_ERR_NULL        = -1,
    CMD_PARSE_ERR_MALFORMED   = -2,
    CMD_PARSE_ERR_TILE_RANGE  = -3,
    CMD_PARSE_ERR_MISSING_ARG = -4,
    CMD_PARSE_ERR_UNKNOWN_CMD = -5,
} cmd_parse_status_t;

typedef enum {
    CMD_KIND_NONE      = 0,
    CMD_KIND_CONFIGURE = 'C',
    CMD_KIND_SET_STATE = 'S',
    CMD_KIND_DIMMER    = 'D',
    CMD_KIND_LEVEL     = 'L',
} cmd_kind_t;

typedef struct {
    cmd_kind_t kind;
    int        tile_id;            /* 0 .. CMD_PARSE_MAX_TILES-1 */

    /* CMD_KIND_CONFIGURE */
    char       icon[CMD_PARSE_ICON_MAX];
    char       name[CMD_PARSE_NAME_MAX];

    /* CMD_KIND_SET_STATE */
    bool       state;

    /* CMD_KIND_DIMMER */
    bool       dimmer_enabled;

    /* CMD_KIND_DIMMER, CMD_KIND_LEVEL — clamped 0..100 */
    uint8_t    level;
} parsed_cmd_t;

/*
 * parse_ha_command — Parse a null-terminated payload into a parsed_cmd_t.
 * Returns CMD_PARSE_OK on success. `out` is zero-initialised on entry and
 * only populated on success. `payload` is NOT modified (parser copies
 * internally).
 *
 * Level values are clamped to [0, 100]. Names longer than NAME_MAX are
 * truncated (NUL-terminated). Payloads longer than PAYLOAD_MAX are
 * rejected as CMD_PARSE_ERR_MALFORMED.
 */
cmd_parse_status_t parse_ha_command(const char *payload, parsed_cmd_t *out);

/*
 * cmd_parse_status_str — Human-readable status name (for logs/tests).
 */
const char *cmd_parse_status_str(cmd_parse_status_t s);

#ifdef __cplusplus
}
#endif

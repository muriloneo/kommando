/*
 * Copyright (C) 2026 Murilo Freire
 *
 * This file is part of Kommando firmware.
 *
 * Pure-C parser for the HA command protocol. No ESP-IDF deps.
 */

/* strtok_r / strnlen are POSIX — expose them on glibc/musl host builds.
 * ESP-IDF's newlib already provides them; the macro is a no-op there. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "commands_parse.h"

#include <string.h>
#include <stdlib.h>

static int parse_int_clamped(const char *s, int lo, int hi, int *out)
{
    if (!s || *s == '\0') return -1;
    /* Reject leading whitespace and signs other than '-' for level fields.
     * atoi is permissive; we keep it but cap. */
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s) return -1;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    *out = (int)v;
    return 0;
}

const char *cmd_parse_status_str(cmd_parse_status_t s)
{
    switch (s) {
        case CMD_PARSE_OK:              return "OK";
        case CMD_PARSE_ERR_NULL:        return "NULL";
        case CMD_PARSE_ERR_MALFORMED:   return "MALFORMED";
        case CMD_PARSE_ERR_TILE_RANGE:  return "TILE_RANGE";
        case CMD_PARSE_ERR_MISSING_ARG: return "MISSING_ARG";
        case CMD_PARSE_ERR_UNKNOWN_CMD: return "UNKNOWN_CMD";
    }
    return "UNKNOWN";
}

cmd_parse_status_t parse_ha_command(const char *payload, parsed_cmd_t *out)
{
    if (!payload || !out) return CMD_PARSE_ERR_NULL;

    memset(out, 0, sizeof(*out));

    /* Reject overlong / empty payloads up front */
    size_t plen = strnlen(payload, CMD_PARSE_PAYLOAD_MAX + 1);
    if (plen == 0) return CMD_PARSE_ERR_MALFORMED;
    if (plen > CMD_PARSE_PAYLOAD_MAX) return CMD_PARSE_ERR_MALFORMED;

    /* Work on a local copy so the caller's buffer is untouched and
     * strtok_r can mutate freely. */
    char buf[CMD_PARSE_PAYLOAD_MAX + 1];
    memcpy(buf, payload, plen);
    buf[plen] = '\0';

    char *saveptr = NULL;
    char *cmd     = strtok_r(buf,  ":", &saveptr);
    char *id_str  = strtok_r(NULL, ":", &saveptr);

    if (!cmd || !id_str) return CMD_PARSE_ERR_MALFORMED;
    if (cmd[0] == '\0' || cmd[1] != '\0') {
        /* Commands are single-letter */
        return CMD_PARSE_ERR_UNKNOWN_CMD;
    }

    int tile_id = 0;
    if (parse_int_clamped(id_str, -32768, 32767, &tile_id) != 0) {
        return CMD_PARSE_ERR_MALFORMED;
    }
    if (tile_id < 0 || tile_id >= CMD_PARSE_MAX_TILES) {
        return CMD_PARSE_ERR_TILE_RANGE;
    }
    out->tile_id = tile_id;

    switch (cmd[0]) {
        case 'C': {
            char *icon = strtok_r(NULL, ":", &saveptr);
            char *name = strtok_r(NULL, "",  &saveptr); /* rest of string */
            if (!icon || icon[0] == '\0') return CMD_PARSE_ERR_MISSING_ARG;

            out->kind = CMD_KIND_CONFIGURE;
            strncpy(out->icon, icon, CMD_PARSE_ICON_MAX - 1);
            out->icon[CMD_PARSE_ICON_MAX - 1] = '\0';
            if (name) {
                strncpy(out->name, name, CMD_PARSE_NAME_MAX - 1);
                out->name[CMD_PARSE_NAME_MAX - 1] = '\0';
            }
            return CMD_PARSE_OK;
        }
        case 'S': {
            char *st = strtok_r(NULL, ":", &saveptr);
            if (!st || st[0] == '\0') return CMD_PARSE_ERR_MISSING_ARG;
            out->kind  = CMD_KIND_SET_STATE;
            out->state = (strcmp(st, "1") == 0);
            return CMD_PARSE_OK;
        }
        case 'D': {
            char *en = strtok_r(NULL, ":", &saveptr);
            char *lv = strtok_r(NULL, ":", &saveptr);
            if (!en || !lv) return CMD_PARSE_ERR_MISSING_ARG;
            int e = 0, l = 0;
            if (parse_int_clamped(en, 0, 1,   &e) != 0) return CMD_PARSE_ERR_MALFORMED;
            if (parse_int_clamped(lv, 0, 100, &l) != 0) return CMD_PARSE_ERR_MALFORMED;
            out->kind           = CMD_KIND_DIMMER;
            out->dimmer_enabled = (e != 0);
            out->level          = (uint8_t)l;
            return CMD_PARSE_OK;
        }
        case 'L': {
            char *lv = strtok_r(NULL, ":", &saveptr);
            if (!lv) return CMD_PARSE_ERR_MISSING_ARG;
            int l = 0;
            if (parse_int_clamped(lv, 0, 100, &l) != 0) return CMD_PARSE_ERR_MALFORMED;
            out->kind  = CMD_KIND_LEVEL;
            out->level = (uint8_t)l;
            return CMD_PARSE_OK;
        }
        default:
            return CMD_PARSE_ERR_UNKNOWN_CMD;
    }
}

/*
 * Copyright (C) 2026 Murilo Freire
 *
 * commands_dedupe.c — see header.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "commands_dedupe.h"

#include <string.h>

void cmd_dedupe_reset(cmd_dedupe_cache_t *cache)
{
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
}

bool cmd_dedupe_should_drop(cmd_dedupe_cache_t *cache, const char *payload)
{
    if (!cache || !payload) return false;

    parsed_cmd_t parsed;
    if (parse_ha_command(payload, &parsed) != CMD_PARSE_OK) return false;
    if (parsed.kind != CMD_KIND_CONFIGURE)                  return false;
    if (parsed.tile_id < 0 || parsed.tile_id >= CMD_PARSE_MAX_TILES) return false;

    char *slot = cache->last[parsed.tile_id];
    if (slot[0] != '\0' && strncmp(slot, payload, CMD_DEDUPE_ENTRY_MAX) == 0) {
        return true;
    }
    /* Store (truncating) for next comparison. */
    size_t n = strnlen(payload, CMD_DEDUPE_ENTRY_MAX - 1);
    memcpy(slot, payload, n);
    slot[n] = '\0';
    return false;
}

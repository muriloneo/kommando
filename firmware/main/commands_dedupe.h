/*
 * Copyright (C) 2026 Murilo Freire
 *
 * commands_dedupe.h — Pure-C duplicate-detection cache for C: configure
 * payloads. Prevents z2m echo → HA → device loops.
 *
 * No ESP-IDF deps. Host-testable.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "commands_parse.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CMD_DEDUPE_ENTRY_MAX
#define CMD_DEDUPE_ENTRY_MAX 64
#endif

typedef struct {
    char last[CMD_PARSE_MAX_TILES][CMD_DEDUPE_ENTRY_MAX];
} cmd_dedupe_cache_t;

/* Zero the cache. */
void cmd_dedupe_reset(cmd_dedupe_cache_t *cache);

/*
 * Returns true if `payload` is a CMD_KIND_CONFIGURE for a valid tile id
 * AND is byte-identical to the previously seen payload for that tile.
 * Otherwise returns false and (if it's a valid C: payload) updates the
 * cache so the next identical payload will be detected.
 *
 * Non-C payloads, NULLs, parse errors, and out-of-range tile ids all
 * return false without mutating the cache.
 */
bool cmd_dedupe_should_drop(cmd_dedupe_cache_t *cache, const char *payload);

#ifdef __cplusplus
}
#endif

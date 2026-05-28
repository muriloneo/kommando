/*
 * Pure-C tests for commands_dedupe.
 */
#include "commands_dedupe.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int g_assertions = 0;
#define CHECK(cond) do { ++g_assertions; assert(cond); } while (0)

static void test_basic_dedupe(void)
{
    cmd_dedupe_cache_t cache;
    cmd_dedupe_reset(&cache);

    /* First time: not a duplicate, cache populated */
    CHECK(cmd_dedupe_should_drop(&cache, "C:0:bulb:Lamp") == false);
    /* Second identical: dropped */
    CHECK(cmd_dedupe_should_drop(&cache, "C:0:bulb:Lamp") == true);
    /* Different payload for same tile: not dropped, cache updated */
    CHECK(cmd_dedupe_should_drop(&cache, "C:0:wifi:Router") == false);
    /* Old payload no longer cached */
    CHECK(cmd_dedupe_should_drop(&cache, "C:0:bulb:Lamp") == false);
    /* New cached now */
    CHECK(cmd_dedupe_should_drop(&cache, "C:0:bulb:Lamp") == true);
}

static void test_independent_tiles(void)
{
    cmd_dedupe_cache_t cache;
    cmd_dedupe_reset(&cache);

    CHECK(cmd_dedupe_should_drop(&cache, "C:0:bulb:A") == false);
    CHECK(cmd_dedupe_should_drop(&cache, "C:1:bulb:A") == false);
    CHECK(cmd_dedupe_should_drop(&cache, "C:2:bulb:A") == false);
    /* Each tile remembers its own */
    CHECK(cmd_dedupe_should_drop(&cache, "C:0:bulb:A") == true);
    CHECK(cmd_dedupe_should_drop(&cache, "C:1:bulb:A") == true);
    CHECK(cmd_dedupe_should_drop(&cache, "C:2:bulb:A") == true);
}

static void test_non_configure_never_drops(void)
{
    cmd_dedupe_cache_t cache;
    cmd_dedupe_reset(&cache);

    /* S/D/L are not subject to dedupe */
    CHECK(cmd_dedupe_should_drop(&cache, "S:0:1") == false);
    CHECK(cmd_dedupe_should_drop(&cache, "S:0:1") == false);
    CHECK(cmd_dedupe_should_drop(&cache, "D:0:1:50") == false);
    CHECK(cmd_dedupe_should_drop(&cache, "L:0:80") == false);
    CHECK(cmd_dedupe_should_drop(&cache, "L:0:80") == false);
}

static void test_invalid_inputs(void)
{
    cmd_dedupe_cache_t cache;
    cmd_dedupe_reset(&cache);

    CHECK(cmd_dedupe_should_drop(NULL, "C:0:bulb:X") == false);
    CHECK(cmd_dedupe_should_drop(&cache, NULL) == false);
    CHECK(cmd_dedupe_should_drop(&cache, "") == false);
    CHECK(cmd_dedupe_should_drop(&cache, "garbage") == false);
    CHECK(cmd_dedupe_should_drop(&cache, "C:99:bulb:X") == false);   /* out of range */
    CHECK(cmd_dedupe_should_drop(&cache, "C:0") == false);            /* missing args */
}

static void test_reset_clears(void)
{
    cmd_dedupe_cache_t cache;
    cmd_dedupe_reset(&cache);
    CHECK(cmd_dedupe_should_drop(&cache, "C:3:home:Hub") == false);
    CHECK(cmd_dedupe_should_drop(&cache, "C:3:home:Hub") == true);
    cmd_dedupe_reset(&cache);
    CHECK(cmd_dedupe_should_drop(&cache, "C:3:home:Hub") == false);
}

int main(void)
{
    test_basic_dedupe();
    test_independent_tiles();
    test_non_configure_never_drops();
    test_invalid_inputs();
    test_reset_clears();
    printf("%d/%d assertions passed\n", g_assertions, g_assertions);
    return 0;
}

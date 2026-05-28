/*
 * Pure-C host tests for commands_parse. Build with:
 *   gcc -std=c11 -Wall -Wextra -Werror -O0 -g \
 *       firmware/host_test/test_commands_parse.c \
 *       firmware/main/commands_parse.c \
 *       -Ifirmware/main -Ifirmware/host_test -o build_host/test_commands_parse
 *   ./build_host/test_commands_parse
 *
 * No frameworks. Assert + a tiny runner = under 5KB compiled.
 */

#include "commands_parse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static int g_total    = 0;

#define EXPECT(cond, msg) do { \
    g_total++; \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, msg); \
    } \
} while (0)

#define EXPECT_EQ_INT(a, b, msg) do { \
    g_total++; \
    long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { \
        g_failures++; \
        fprintf(stderr, "FAIL %s:%d  %s (got %ld, want %ld)\n", \
                __FILE__, __LINE__, msg, _a, _b); \
    } \
} while (0)

#define EXPECT_EQ_STR(a, b, msg) do { \
    g_total++; \
    if (strcmp((a), (b)) != 0) { \
        g_failures++; \
        fprintf(stderr, "FAIL %s:%d  %s (got '%s', want '%s')\n", \
                __FILE__, __LINE__, msg, (a), (b)); \
    } \
} while (0)

/* ---------- Happy paths ---------- */

static void test_configure_full(void)
{
    parsed_cmd_t p;
    cmd_parse_status_t s = parse_ha_command("C:0:bulb:Kitchen Light", &p);
    EXPECT_EQ_INT(s, CMD_PARSE_OK, "C: ok");
    EXPECT_EQ_INT(p.kind, CMD_KIND_CONFIGURE, "C: kind");
    EXPECT_EQ_INT(p.tile_id, 0, "C: tile");
    EXPECT_EQ_STR(p.icon, "bulb", "C: icon");
    EXPECT_EQ_STR(p.name, "Kitchen Light", "C: name");
}

static void test_configure_name_with_colons(void)
{
    /* Name is the rest of the string — must preserve embedded colons. */
    parsed_cmd_t p;
    cmd_parse_status_t s = parse_ha_command("C:2:power:Hall: lamp:1", &p);
    EXPECT_EQ_INT(s, CMD_PARSE_OK, "C-colons: ok");
    EXPECT_EQ_STR(p.name, "Hall: lamp:1", "C-colons: name preserves ':'");
}

static void test_configure_empty_name(void)
{
    parsed_cmd_t p;
    cmd_parse_status_t s = parse_ha_command("C:1:wifi:", &p);
    EXPECT_EQ_INT(s, CMD_PARSE_OK, "C-empty-name: ok");
    EXPECT_EQ_STR(p.name, "", "C-empty-name: name is empty");
    EXPECT_EQ_STR(p.icon, "wifi", "C-empty-name: icon");
}

static void test_set_state(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command("S:3:1", &p), CMD_PARSE_OK, "S1: ok");
    EXPECT_EQ_INT(p.kind, CMD_KIND_SET_STATE, "S1: kind");
    EXPECT_EQ_INT(p.tile_id, 3, "S1: tile");
    EXPECT(p.state == true, "S1: state on");

    EXPECT_EQ_INT(parse_ha_command("S:3:0", &p), CMD_PARSE_OK, "S0: ok");
    EXPECT(p.state == false, "S0: state off");

    /* Anything other than "1" is treated as off (matches firmware) */
    EXPECT_EQ_INT(parse_ha_command("S:3:yes", &p), CMD_PARSE_OK, "S-text: ok");
    EXPECT(p.state == false, "S-text: state off");
}

static void test_dimmer(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command("D:1:1:42", &p), CMD_PARSE_OK, "D: ok");
    EXPECT_EQ_INT(p.kind, CMD_KIND_DIMMER, "D: kind");
    EXPECT_EQ_INT(p.tile_id, 1, "D: tile");
    EXPECT(p.dimmer_enabled, "D: enabled");
    EXPECT_EQ_INT(p.level, 42, "D: level");
}

static void test_dimmer_clamps_level(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command("D:1:1:250", &p), CMD_PARSE_OK, "D-high: ok");
    EXPECT_EQ_INT(p.level, 100, "D-high: clamped to 100");

    EXPECT_EQ_INT(parse_ha_command("D:1:1:-5", &p), CMD_PARSE_OK, "D-neg: ok");
    EXPECT_EQ_INT(p.level, 0, "D-neg: clamped to 0");
}

static void test_level(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command("L:5:75", &p), CMD_PARSE_OK, "L: ok");
    EXPECT_EQ_INT(p.kind, CMD_KIND_LEVEL, "L: kind");
    EXPECT_EQ_INT(p.level, 75, "L: level");
}

/* ---------- Error paths ---------- */

static void test_null_inputs(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command(NULL, &p), CMD_PARSE_ERR_NULL, "null payload");
    EXPECT_EQ_INT(parse_ha_command("S:0:1", NULL), CMD_PARSE_ERR_NULL, "null out");
}

static void test_empty_payload(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command("", &p), CMD_PARSE_ERR_MALFORMED, "empty");
}

static void test_oversize_payload(void)
{
    char big[CMD_PARSE_PAYLOAD_MAX + 32];
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command(big, &p), CMD_PARSE_ERR_MALFORMED, "oversize");
}

static void test_malformed(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command("garbage", &p), CMD_PARSE_ERR_MALFORMED, "no colon");
    EXPECT_EQ_INT(parse_ha_command("S:", &p), CMD_PARSE_ERR_MALFORMED, "trailing colon only");
}

static void test_tile_range(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command("S:99:1", &p), CMD_PARSE_ERR_TILE_RANGE, "tile too high");
    EXPECT_EQ_INT(parse_ha_command("S:-1:1", &p), CMD_PARSE_ERR_TILE_RANGE, "tile negative");
    EXPECT_EQ_INT(parse_ha_command("S:6:1", &p),  CMD_PARSE_ERR_TILE_RANGE,
                  "tile == MAX_TILES is out");
}

static void test_missing_args(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command("C:0:",   &p), CMD_PARSE_ERR_MISSING_ARG, "C no icon");
    EXPECT_EQ_INT(parse_ha_command("S:0",    &p), CMD_PARSE_ERR_MISSING_ARG, "S no state");
    EXPECT_EQ_INT(parse_ha_command("D:0:1",  &p), CMD_PARSE_ERR_MISSING_ARG, "D no level");
    EXPECT_EQ_INT(parse_ha_command("L:0",    &p), CMD_PARSE_ERR_MISSING_ARG, "L no level");
}

static void test_unknown_cmd(void)
{
    parsed_cmd_t p;
    EXPECT_EQ_INT(parse_ha_command("X:0:1", &p), CMD_PARSE_ERR_UNKNOWN_CMD, "unknown X");
    EXPECT_EQ_INT(parse_ha_command("CC:0:bulb:n", &p), CMD_PARSE_ERR_UNKNOWN_CMD,
                  "multi-letter cmd");
}

static void test_name_truncation(void)
{
    /* Build a name longer than CMD_PARSE_NAME_MAX */
    char big[CMD_PARSE_PAYLOAD_MAX + 1];
    int n = snprintf(big, sizeof(big), "C:0:bulb:");
    int room = (int)sizeof(big) - 1 - n;
    if (room > 0) {
        memset(big + n, 'A', (size_t)room);
        big[sizeof(big) - 1] = '\0';
    }
    parsed_cmd_t p;
    cmd_parse_status_t s = parse_ha_command(big, &p);
    EXPECT_EQ_INT(s, CMD_PARSE_OK, "trunc: ok");
    EXPECT(p.name[CMD_PARSE_NAME_MAX - 1] == '\0', "trunc: NUL-terminated");
    EXPECT((int)strlen(p.name) <= CMD_PARSE_NAME_MAX - 1, "trunc: <= NAME_MAX-1");
}

static void test_payload_not_mutated(void)
{
    /* Caller buffer must be untouched (firmware passes NVS strings). */
    char fixed[] = "C:0:bulb:Hall";
    char copy[sizeof(fixed)];
    memcpy(copy, fixed, sizeof(fixed));
    parsed_cmd_t p;
    parse_ha_command(fixed, &p);
    EXPECT(memcmp(fixed, copy, sizeof(fixed)) == 0,
           "payload buffer not mutated");
}

int main(void)
{
    test_configure_full();
    test_configure_name_with_colons();
    test_configure_empty_name();
    test_set_state();
    test_dimmer();
    test_dimmer_clamps_level();
    test_level();
    test_null_inputs();
    test_empty_payload();
    test_oversize_payload();
    test_malformed();
    test_tile_range();
    test_missing_args();
    test_unknown_cmd();
    test_name_truncation();
    test_payload_not_mutated();

    printf("%d/%d assertions passed\n", g_total - g_failures, g_total);
    return g_failures == 0 ? 0 : 1;
}

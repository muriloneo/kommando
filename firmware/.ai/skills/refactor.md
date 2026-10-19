# Kommando Nano — Refactor Skill

Refactor Kommando Nano firmware (ESP32-C6) safely and with measurable justification.

## Project context

Kommando Nano is a Zigbee touchscreen wall panel. The firmware is ESP-IDF v6.0-beta2, modular by design. Key stability constraints:

- `config.h` is the single source of truth for all GPIO pins, ZCL IDs, OTA macros — never inline these values elsewhere
- ZCL cluster `0xFC11` attribute IDs (`0x0000`–`0x0016`) and command protocol (`C:`, `S:`, `D:`, `N:`, `B:`) are contractual — Zigbee2MQTT external converter and HA blueprint depend on them; changing them is a breaking change
- Tile report format `T:<id>:<state>` on attribute `0x0001` is also contractual
- GPIO4 (Touch INT) must stay on GPIO 0–7 for deep-sleep ext0 wake on ESP32-C6
- Touch RST (GPIO20) reset sequence (LOW → 10 ms → HIGH → 50 ms) in `display.c` is hardware-required
- OTA state machine in `zigbee.c` (`s_ota_partition`, `s_ota_handle`, `zb_ota_reset_state()`) must remain safe on any error path — no partial boot
- `app_update` must stay in CMakeLists REQUIRES for OTA link
- LVGL runs in `lcd_task`; Zigbee in `zigbee_task`; tile events flow via `s_tile_event_queue` — do not collapse these task boundaries

## Only refactor when there is a measurable gain

- Reduced code duplication
- Lower stack or heap usage (verify with `idf.py size-components`)
- Improved readability without adding abstraction for its own sake
- Eliminating a known fragility (e.g., magic numbers, silent failures)

## Constraints

- Preserve all hardware pin assignments — only change via `config.h`
- Do not change module public APIs without updating all callers in one diff
- Verify no behavioral change in LVGL rendering, touch handling, or Zigbee reporting
- Keep diffs small and reviewable — one concern per PR

## Approach

1. State the measurable gain with evidence (size diff, duplication count, etc.)
2. List all changed files
3. Highlight behavioral risks — especially any touching Zigbee, OTA, or display init
4. Provide before/after for changed signatures or data structures
5. Compile and size check: `idf.py build && idf.py size-components`

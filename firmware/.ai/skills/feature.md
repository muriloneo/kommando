# Kommando Nano — Feature Skill

Design new firmware features for Kommando Nano (ESP32-C6 Zigbee touchscreen wall panel).

## Project context

Kommando Nano displays 6 configurable touch tiles on a 172×320 ST7789V2 display. Each tile maps to a Home Assistant entity (light, switch, scene, etc.), controlled bidirectionally over Zigbee via Zigbee2MQTT.

**Module map** (`firmware/main/`):

| Module | Role |
|---|---|
| `config.h` | Single source of truth: all GPIOs, Zigbee IDs, OTA macros, firmware version — always read before any change |
| `main.c` | Entry point; starts `lcd_task` (LVGL + UI) and `zigbee_task` as FreeRTOS tasks |
| `zigbee.c/.h` | Zigbee init, join/re-join state machine, ZCL attribute storage, HA command receive, tile-tap reporting, OTA client, NVS sync |
| `commands.c/.h` | HA → device command parser; protocol: `C:<id>:<icon>:<name>`, `S:<id>:<0\|1>`, `D:<id>:<0\|1>:<level>`, `N:<0\|1>`, `B:<0..255>`; duplicate suppression |
| `tiles.c/.h` | `ui_tile_t` descriptor, LVGL tap/long-press callbacks, FreeRTOS event queue, `tile_report_task` that sends `T:<id>:<state>` over Zigbee |
| `display.c/.h` | SPI LCD + I2C touch init, LVGL port setup, `display_timeout_task` (dim + deep-sleep state machine) |
| `ui_panel.c/.h` | LVGL UI: 2×3 tile grid, header bar, connection overlay, night-mode scheme, dimmer badge + slider |
| `backlight.c/.h` | LEDC PWM backlight: `backlight_init()`, `backlight_set(uint8_t)`, `backlight_get()` |
| `led.c/.h` | WS2812 single-pixel status LED via `espressif__led_strip` |
| `button.c/.h` | GPIO9: short press = wake, 5 s = re-pair, 10 s = factory reset |

**Zigbee cluster**: custom `0xFC11` (manuf `0x1234`) on endpoint 1; endpoints 2–6 expose OnOff only.
**Settings** (attr `0x0010`–`0x0016`): backlight, dim timeout, dim level, night mode, night brightness, deep sleep enable, deep sleep timeout — all R/W, persisted to NVS.

## Rules

- Never suggest Arduino framework — ESP-IDF native APIs only
- Never change GPIO assignments without updating `config.h` as the only source of truth
- Touch INT must remain on a GPIO 0–7 (deep-sleep ext0 wake constraint on ESP32-C6)
- Keep memory footprint low — no heap fragmentation; prefer static allocation
- Preserve low-power behavior: screen dim + deep sleep state machine in `display.c`
- Don't break existing ZCL attribute IDs or command protocol — Z2M converter and HA blueprint depend on them

## Approach

1. Identify which modules are touched
2. Check `config.h` for any relevant pin/ID constants
3. Define the minimal interface change
4. Sketch implementation with affected files
5. Flag memory, power, and Zigbee compatibility risks
6. Compile command: `idf.py build`

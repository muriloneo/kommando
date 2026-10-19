# Kommando Nano — Debug Skill

Analyze ESP-IDF errors and firmware issues on the Kommando Nano (ESP32-C6 Zigbee touchscreen wall panel).

## Project context

- MCU: ESP32-C6, framework: ESP-IDF v6.0-beta2 (never Arduino)
- Display: ST7789V2 172×320 SPI; Touch: CST816S I2C
- GPIO canonical mapping is in `main/config.h` — never guess pin numbers
  - Backlight: GPIO2 (LEDC PWM), MOSI: GPIO7, CLK: GPIO6, CS: GPIO10, DC: GPIO11, LCD RST: GPIO3
  - Touch SDA: GPIO0, SCL: GPIO1, INT: GPIO4 (deep-sleep wake, must stay GPIO 0–7), Touch RST: GPIO20
  - RGB LED: GPIO8 (WS2812), Button: GPIO9 (active low)
- Touch RST is driven manually in `display.c` (LOW → 10 ms → HIGH → 50 ms); driver receives `GPIO_NUM_NC`
- LVGL UI lives in `ui_panel.c`; tile state in `tiles.c`; Zigbee in `zigbee.c`
- OTA: dual-bank (`ota_0`/`ota_1` 1.5 MB each), client state machine in `zigbee.c`
- Zigbee reporting: `esp_zb_zcl_update_reporting_info` silently fails if no slot exists; use `zb_zcl_put_reporting_info()` directly
- `app_update` must be in CMakeLists REQUIRES (not auto-linked) for `esp_ota_ops.h` to link

## Checklist

- Build errors: CMake, ninja, linker — check REQUIRES in `main/CMakeLists.txt`
- Stack/heap overflows: LVGL tasks, `lcd_task`, `zigbee_task`, `tile_report_task`, `display_timeout_task`
- LVGL rendering: `ui_panel.c`, port config in `display.c`, LVGL port component `espressif__esp_lvgl_port`
- GPIO conflicts: cross-reference `main/config.h` before any pin change
- Touch issues: I2C init in `display.c`, CST816S driver `espressif__esp_lcd_touch_cst816s`, INT on GPIO4
- Deep sleep regressions: ext0 wake source must be GPIO 0–7; `display_timeout_task` state machine
- Zigbee/ZBOSS errors: `zigbee.c`, cluster `0xFC11`, manuf `0x1234`, endpoint 1 + endpoints 2–6 (OnOff)
- OTA failures: `zb_ota_reset_state(true)` on any error; check `s_ota_in_progress`, `s_ota_tagid_received`; verify partition table offsets in `partition_table/partition-table.csv`
- NVS issues: settings persisted in `zigbee.c` NVS sync; factory reset via 10 s button hold

## Approach

1. State root cause first
2. List affected files
3. Provide minimal fix — no rewrites of working modules
4. Compile command to verify: `idf.py -p <PORT> build flash monitor`

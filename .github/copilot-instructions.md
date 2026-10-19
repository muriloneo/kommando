# RTK — Token-Optimized CLI

**rtk** is a CLI proxy that filters and compresses command outputs, saving 60-90% tokens.

## Rule

Always prefix shell commands with `rtk`:

```bash
# Instead of:              Use:
git status                 rtk git status
git log -10                rtk git log -10
cargo test                 rtk cargo test
docker ps                  rtk docker ps
kubectl get pods           rtk kubectl pods
```

## Meta commands (use directly)

```bash
rtk gain              # Token savings dashboard
rtk gain --history    # Per-command savings history
rtk discover          # Find missed rtk opportunities
rtk proxy <cmd>       # Run raw (no filtering) but track usage
```

---

# Kommando Monorepo

This is a monorepo containing multiple projects:

- **firmware/** — ESP32-C6 Zigbee touchscreen wall panel firmware (ESP-IDF)
- **z2m-converter/** — Zigbee2MQTT external converter (JavaScript)
- **home-assistant-blueprint/** — Home Assistant automation blueprint
- **ki-kad/** — KiCad hardware design files
- **docs/** — Project documentation

## Opening Firmware in VS Code

When working on firmware, open the **firmware/** subfolder as the workspace:

```bash
git clone https://github.com/muriloneo/kommando.git
cd kommando/firmware
code .
```

The firmware folder has its own `.github/copilot-instructions.md` with firmware-specific context.
Git operations work correctly because the repository root (.git) is found automatically at the parent level.

---

---

# Kommando Firmware

You are working on Kommando — an ESP32-C6 smart remote firmware.

## Stack

- **MCU**: ESP32-C6
- **Framework**: ESP-IDF (native — never Arduino)
- **UI**: LVGL with `espressif__esp_lvgl_port`
- **Touch**: CST816S via `espressif__esp_lcd_touch_cst816s`
- **Display**: custom display drivers (`main/display.c`)
- **Wireless**: Zigbee (ZBOSS via `espressif__esp-zigbee-lib`)
- **LED**: addressable LED strip (`espressif__led_strip`)
- **OTA**: custom OTA pipeline (`tools/`, `releases/`)

## Rules

- Never suggest Arduino framework
- Prefer ESP-IDF native APIs
- Preserve low power usage
- Keep memory usage low — avoid heap fragmentation
- Do not break display/touch pin mappings (see `main/config.h`)
- Prefer modular architecture — one concern per file

## Before suggesting changes

1. Read `.ai/repo.xml` or `repo-context.md` for current architecture
2. Understand which modules are affected
3. Suggest minimal diffs

## Always output

- Impacted files
- Risks (memory, power, hardware compatibility)
- Compile command to verify: `idf.py build`

## Skill prompts

Reusable prompt context lives in `.ai/skills/`:
- `debug.md` — diagnose build/runtime/hardware issues
- `feature.md` — design new features safely
- `refactor.md` — refactor with measurable justification

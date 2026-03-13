# Installation Guide

This guide covers a full local setup for **firmware + Zigbee2MQTT + Home Assistant**.

## 1) Requirements

- ESP32-C6 hardware panel (ST7789 + CST816S)
- USB cable and serial access
- Zigbee coordinator with Zigbee2MQTT
- Home Assistant (recommended)
- Windows / Linux / macOS development machine

### Software

- ESP-IDF v6.0-beta2 (same version used by this project)
- Python 3.10+
- Node.js 18+ (for converter ecosystem tooling)
- Zigbee2MQTT

## 2) Build firmware

From repository root:

1. Open ESP-IDF shell
2. Change into the firmware folder:

```bash
cd firmware
```

3. Build:

```bash
idf.py build
```

4. Flash and monitor:

```bash
idf.py flash monitor
```

### Optional: flash a prebuilt release (no local build)

If you do **not** want to build firmware locally, you can download a prebuilt
`kommando_webflash.bin` from project releases and flash it directly in the browser.

Even though this project does **not** use ESPHome firmware, the ESP Web Tools UI
(`web.esphome.io`) can still flash standard ESP-IDF binaries.

Release binary path (in this repo):

- `firmware/releases/kommando_webflash.bin`

Web flasher:

- https://web.esphome.io/

## 3) Install Zigbee2MQTT converter

Copy the runtime converter from this repo to your Z2M data folder:

- Source: `z2m-converter/kommando_nano.js`
- Destination: `<z2m-data>/kommando_nano.js`

Then update Zigbee2MQTT `configuration.yaml`:

```yaml
external_converters:
  - kommando_nano.js
```

Restart Zigbee2MQTT.

## 4) Pair device

- Put Zigbee2MQTT in permit-join mode
- Hold panel hardware button for 5 seconds (pairing mode)
- Confirm device appears in Z2M with model `Kommando_Nano`

## 5) Install Home Assistant blueprint

- Source file in repo: `home-assistant-blueprint/kommando_panel.yaml`
- Import into Home Assistant Blueprints
- Create automation instance and map entities for all tiles

## 6) Validate end-to-end

- Tap tile → HA entity changes
- Change HA entity state → panel tile state updates
- Long press / dim gesture → brightness updates in HA

## Troubleshooting

- If converter is not loaded: check Zigbee2MQTT logs for syntax errors and path issues
- If startup sync is wrong: check blueprint debug topic payloads (`.../debug`)
- If `@flash_args` fails in PowerShell, use the explicit merge command above from `firmware/build`.

## TODO

- Add a GitHub release pipeline to automatically build and publish
  `kommando_webflash.bin` for each release/tag.

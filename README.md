# Kommando Nano (ESP32-C6 Zigbee Touch Panel)

Open-source firmware + integrations for a 6-tile Zigbee wall panel based on **ESP32-C6**, including:

- ESP-IDF firmware
- Zigbee2MQTT external converter
- Home Assistant blueprint automation

---

## What this project does

- 6 touch tiles with per-tile icon/name/entity mapping
- Bidirectional state sync:
  - Panel tap/slider -> Home Assistant entity
  - Home Assistant change -> panel visual state
- Dimmer support per tile (for light domains)
- Runtime settings over Zigbee custom cluster `0xFC11`:
  - backlight
  - screen timeout
  - dim level
  - night mode / night brightness
  - deep sleep / sleep timeout

---

## Repository layout

- `firmware/` - ESP-IDF firmware project root
- `z2m-converter/kommando_nano.js` - Zigbee2MQTT external converter
- `home-assistant-blueprint/kommando_panel.yaml` - Home Assistant blueprint
- `assets/images/` - screenshots / visuals TODO
- `assets/videos/` - demo videos TODO

---

## Requirements

### Hardware

- ESP32-C6 based panel (ST7789 + CST816S)
- MCU board (AliExpress Affiliate link): https://s.click.aliexpress.com/e/_c3c6hznv
- 1.9-inch LCD + touch (ST7789 170x320, AliExpress Affiliate link): https://s.click.aliexpress.com/e/_c4EcqUaF
- USB cable and serial access
- Zigbee coordinator with Zigbee2MQTT
- Home Assistant (recommended)

### Software

- ESP-IDF v6.0-beta2
- Python 3.10+
- Node.js 18+
- Zigbee2MQTT

---

## Installation (firmware + Z2M + HA)

### 1) Build firmware

From repo root, open ESP-IDF shell and run:

```bash
cd firmware
idf.py build
idf.py flash monitor
```

### Optional: flash prebuilt release binary

If you prefer no local build, flash the prebuilt binary:

- Binary path in repo: `firmware/releases/kommando_webflash.bin`
- Web flasher: https://web.esphome.io/

### 2) Install Zigbee2MQTT converter

Copy:

- From: `z2m-converter/kommando_nano.js`
- To: `<z2m-data>/external_converters/kommando_nano.js`

Then in Zigbee2MQTT `configuration.yaml`:

```yaml
external_converters:
  - kommando_nano.js
```

Restart Zigbee2MQTT.

### 3) Pair device

- Enable permit-join in Zigbee2MQTT
- Hold panel hardware button for 5 seconds (pair mode)
- Confirm model appears as `Kommando_Nano`

### 4) Import Home Assistant blueprint

- Local file: `home-assistant-blueprint/kommando_panel.yaml`

Direct import link (fixed):

- https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https://raw.githubusercontent.com/muriloneo/kommando/main/home-assistant-blueprint/kommando_panel.yaml

After import, create an automation instance and map entities for each tile.

### 5) Validate end-to-end

- Tap tile -> HA entity changes
- Change HA entity -> panel tile state updates
- Long press / dim gesture -> brightness updates

---

## Copy map (exact deploy paths)

### Zigbee2MQTT

- Copy: `z2m-converter/kommando_nano.js`
- Destination: `<z2m-data>/external_converters/kommando_nano.js`
- Config (`configuration.yaml`):

```yaml
external_converters:
  - kommando_nano.js
```

### Home Assistant

- Blueprint file: `home-assistant-blueprint/kommando_panel.yaml`
- Import into Blueprints and create automation from it

### Firmware

- Build from `firmware/`
- Flash using `idf.py flash monitor`

---

## Hardware summary

| Component | Part | Notes |
|---|---|---|
| MCU | ESP32-C6 | Zigbee 802.15.4 firmware. AliExpress Affiliate link: https://s.click.aliexpress.com/e/_c3c6hznv |
| LCD | ST7789 | 172x320 over SPI. AliExpress Affiliate link (1.9" LCD + touch module): https://s.click.aliexpress.com/e/_c4EcqUaF |
| Touch | CST816S | I2C capacitive touch. Same AliExpress Affiliate link as LCD module: https://s.click.aliexpress.com/e/_c4EcqUaF |
| RGB LED | WS2812 | status/pairing feedback |
| Button | GPIO9 | pairing/reset actions |

Pin map is in headers under `firmware/main/`.

---

## Device identity

- Manufacturer: `Kommando`
- Model: `Kommando_Nano`

Legacy fingerprints remain in converter for older flashed units.

---

## Troubleshooting

- Converter not detected: verify `external_converters` path and restart Zigbee2MQTT.
- Blueprint imported but no action: verify MQTT topic and entity mapping.
- Startup sync issues: inspect `.../debug` topic payloads.
- PowerShell `@flash_args` issue: run flash command from `firmware/build` with explicit merge arguments.

---

## Support the project ☕

If this project helps you, you can support development here:

- https://buymeacoffee.com/kommando

QR code:

![Buy Me a Coffee QR](assets/images/buymeacoffee_kommando_qr.png)

---

## License

This project is licensed under the **GNU General Public License v3.0**.

See `LICENSE` for details.

---

## TODO

- [ ] Fix link for importing blueprint directly
- [ ] Add OTA
- [ ] Add GitHub pipeline for releases


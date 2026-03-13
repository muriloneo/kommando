# Kommando Nano (ESP32-C6 Zigbee Touch Panel)

Open-source firmware + integrations for a 6-tile Zigbee wall panel based on **ESP32-C6**, with:

- ESP-IDF firmware
- Zigbee2MQTT external converter
- Home Assistant blueprint automation

This repository is structured so you can build the firmware and deploy integrations quickly.

---

## What this project does

- 6 touch tiles with per-tile icon/name/entity mapping
- Bidirectional state sync:
  - Panel tap/slider -> Home Assistant entity
  - Home Assistant change -> panel visual state
- Dimmer support per tile (for light domains)
- Panel runtime settings exposed over Zigbee custom cluster `0xFC11`:
  - backlight
  - screen timeout
  - dim level
  - night mode / night brightness
  - deep sleep / sleep timeout

---

## Repository layout

Key folders:

- `firmware/` - ESP-IDF firmware project root
- `z2m-converter/kommando_nano.js` - deployable Z2M external converter
- `home-assistant-blueprint/kommando_panel.yaml` - importable HA blueprint
- `docs/INSTALL.md` - full installation walkthrough
- `docs/COPY_MAP.md` - exact “copy file X -> location Y” map
- `docs/MEDIA_GUIDE.md` - screenshots/video shot list and placeholders

---

## Quick start (5 steps)

1. Build and flash firmware from `firmware/`.
2. Copy `z2m-converter/kommando_nano.js` into your Zigbee2MQTT data directory.
3. Register converter in Zigbee2MQTT `configuration.yaml`.
4. Import Home Assistant blueprint from `home-assistant-blueprint/kommando_panel.yaml`.
5. Pair the device and map each tile to entities.

Detailed steps: `docs/INSTALL.md`.

---

## Exact integration copy instructions

### Zigbee2MQTT

Copy:

- `z2m-converter/kommando_nano.js`

To your Zigbee2MQTT data path, then set:

```yaml
external_converters:
  - kommando_nano.js
```

### Home Assistant

Import blueprint file:

- `home-assistant-blueprint/kommando_panel.yaml`

Then create an automation instance from that blueprint.

More details: `docs/COPY_MAP.md`.

---

## Firmware build

This project targets ESP-IDF (tested with v6.0-beta2).

```bash
cd firmware
idf.py build
idf.py flash monitor
```

---

## Hardware summary

| Component | Part | Notes |
|---|---|---|
| MCU | ESP32-C6 | Zigbee 802.15.4 router end-device firmware |
| LCD | ST7789 | 172x320 over SPI |
| Touch | CST816S | I2C capacitive touch |
| RGB LED | WS2812 | status/pairing feedback |
| Button | GPIO9 | pairing/reset actions |

Pin map is defined in firmware config headers under `firmware/main/`.

---

## Media placeholders

Add screenshots/videos under `assets/` and embed them in this README.

Recommended file names and shot list are in `docs/MEDIA_GUIDE.md`.

Placeholder examples:

```md
![Panel hero](assets/images/hero_panel.jpg)
![HA blueprint setup](assets/images/ha_blueprint_import.png)

[Pairing demo video](assets/videos/pairing_demo.mp4)
```

---

## Device identity

Canonical Zigbee identity used by this project:

- Manufacturer: `Kommando`
- Model: `Kommando_Nano`

Legacy fingerprints are retained in the converter for backward compatibility with older flashed units.

---

## Troubleshooting

- Converter not detected: verify `external_converters` path and restart Zigbee2MQTT.
- Blueprint imported but no action: verify MQTT topic and entity mapping in automation instance.
- Startup states wrong: check panel `.../debug` topic in MQTT explorer and confirm HA entities are available after boot.

---

## Additional docs

- `docs/INSTALL.md`
- `docs/COPY_MAP.md`
- `docs/MEDIA_GUIDE.md`
- `firmware/SYSTEM_PROMPT.md`
- `firmware/OTA_IMPLEMENTATION.md`

---

## License

MIT


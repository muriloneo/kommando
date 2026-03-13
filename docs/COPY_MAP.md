# Copy Map: What goes where

Use this when deploying to your own stack.

## Zigbee2MQTT

- Copy: `z2m-converter/kommando_nano.js`
- To: `<z2m-data>/external_converters/kommando_nano.js`
- Then in `configuration.yaml`:

```yaml
external_converters:
  - kommando_nano.js
```

## Home Assistant

- Import blueprint from:
  - `home-assistant-blueprint/kommando_panel.yaml`
- Create automation from imported blueprint

## Firmware

- Build from the `firmware/` subfolder (ESP-IDF project root)
- Flash to device with:

```bash
cd firmware
idf.py flash monitor
```

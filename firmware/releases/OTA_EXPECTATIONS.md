# Kommando OTA Expectations (Zigbee2MQTT)

This document explains **what should happen** with the current firmware OTA implementation and whether it matches the expected workflow:

> "Z2M sends firmware and the device updates itself from inside."

---

## Short answer

**Yes — functionally, this is what now happens.**

With the current implementation, your device acts as a Zigbee OTA client and updates itself internally using ESP-IDF OTA APIs (`esp_ota_begin/write/end`, set boot partition, reboot).

From your perspective in Zigbee2MQTT, you trigger OTA and the update is delivered wirelessly.

---

## Important architecture detail (push vs pull)

Technically, Zigbee OTA is a **client-driven pull** process:

1. Device joins Zigbee network.
2. Device discovers OTA server (Match Descriptor).
3. Device sends "Query Next Image".
4. Coordinator/OTA server provides metadata and image blocks.
5. Device writes blocks to inactive OTA partition.
6. Device validates, switches boot partition, reboots.

So it is not a raw MQTT push of a `.bin` directly into the ESP over Zigbee commands.

What is sent over Zigbee is a **Zigbee OTA image** (`.ota`) in OTA cluster protocol.

---

## What you should expect in practice

## 1) Build/output

- Firmware builds successfully.
- Partition table includes:
  - `otadata`
  - `ota_0`
  - `ota_1`
- Firmware binary fits in partition (already validated in build logs).

## 2) Device behavior after joining

After join/rejoin success, firmware schedules OTA server discovery and image query.

Expected logs include messages like:

- OTA server discovered
- Query OTA image from server
- OTA image found
- OTA receive progress
- OTA finish / restarting

## 3) Acceptance rules for an update

The firmware will accept OTA only if these conditions pass:

- `manufacturerCode` matches firmware config (`0x1234` by default)
- `imageType` matches firmware config (`0x1011` by default)
- `fileVersion` is **newer** than currently running version

If any mismatch occurs, update is rejected by design.

## 4) Artifact type required by Z2M

Use a `.ota` file, not raw `.bin`.

Your build tooling already generates `.ota` via `tools/image_builder_tool.py` / `zigbee_ota_image` target.

---

## Configuration values currently expected

Default OTA identity in firmware:

- Manufacturer code: `0x1234` (decimal `4660`)
- Image type: `0x1011` (decimal `4113`)
- HW version: `0x0101`
- Max data size: `223`

If you change these values in Kconfig, your OTA index metadata must match.

---

## Minimal Z2M index expectations

Each firmware entry should contain at least:

- `url` to `.ota`
- `manufacturerCode`
- `imageType`
- `fileVersion`

Optional:

- `sha512`
- `force`

---

## Common reasons OTA appears to "do nothing"

- `manufacturerCode` mismatch
- `imageType` mismatch
- New image `fileVersion` is not greater than running firmware
- `.bin` used instead of `.ota`
- OTA index not loaded by Z2M / cache stale
- Device did not successfully discover OTA server on network

---

## Final expectation check

Your expectation:

- "Z2M sends binary, device updates from inside"

Reality with this firmware:

- ✅ Device updates itself internally (exactly true)
- ✅ Update is initiated and delivered via Zigbee OTA path through coordinator
- ⚠️ Artifact must be Zigbee OTA image (`.ota`), not plain `.bin`

So for deployment behavior, this implementation satisfies your OTA goal.

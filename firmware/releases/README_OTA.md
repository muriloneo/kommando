# Kommando OTA — How to Update Firmware Over Zigbee

The Kommando firmware includes a Zigbee OTA client (cluster `0x0019`, endpoint `1`). This lets
Zigbee2MQTT push a new firmware image to the device wirelessly without physical access.

For the automated build/publish pipeline, see [OTA_PIPELINE.md](OTA_PIPELINE.md).

---

## Prerequisites

- Zigbee2MQTT **1.36+** (OTA cluster support)
- Zigbee coordinator joined and Kommando device paired
- ESP-IDF environment active (for building)
- Python with `zigpy`: `pip install -r tools/requirements-ota.txt`

---

## Step 1 — Build the firmware and generate the `.ota` file

From the `firmware/` directory:

```powershell
# Build + package in one command (PowerShell, recommended)
tools/build_and_package_ota.ps1 -GitHubRepo muriloneo/kommando -GitHubTag v1.0.1

# Or: Python directly
python tools/build_and_package_ota.py --project-dir . --github-repo muriloneo/kommando --github-tag v1.0.1
```

This produces two artifacts in `releases/ota-feed/`:

| File | Purpose |
|------|---------|
| `kommando_1.0.1_0x01000100.ota` | Zigbee OTA image to send to the device |
| `kommando_1.0.1_0x01000100.ota.json` | Sidecar metadata (URL, hashes) |
| `index.json` | Z2M OTA index — list of all published versions |

> The `0x01000100` suffix is the packed file version: `major=01 minor=00 patch=01 build=00`.

To build without rebuilding the firmware binary (e.g. if already built):

```powershell
tools/build_and_package_ota.ps1 -SkipBuild -GitHubRepo muriloneo/kommando -GitHubTag v1.0.1
```

---

## Step 2 — Verify the OTA image identity

The `.ota` file must have matching metadata to be accepted by the device.
Check the `index.json` entry:

```json
{
  "url": "https://github.com/muriloneo/kommando/releases/download/v1.0.1/kommando_1.0.1_0x01000100.ota",
  "manufacturerCode": 4660,
  "imageType": 4113,
  "fileVersion": 16777472,
  "sha512": "...",
  "force": false
}
```

| Field | Value | Notes |
|-------|-------|-------|
| `manufacturerCode` | `4660` (`0x1234`) | Must match firmware `ZB_OTA_MANUFACTURER_CODE` |
| `imageType` | `4113` (`0x1011`) | Must match `CONFIG_KOMMANDO_OTA_IMAGE_TYPE` in sdkconfig |
| `fileVersion` | `>` running version | Device rejects same or older versions |

> The firmware's running file version is `FW_VERSION_MAJOR.MINOR.PATCH` packed as
> `0xAABBCC00`. Current release `1.0.0` → `0x01000000` = `16777216`.

---

## Step 3 — Make the `.ota` file reachable by Z2M

### Option A — GitHub Releases (recommended for production)

1. Create a GitHub Release tagged `v1.0.1` on `muriloneo/kommando`.
2. Upload `kommando_1.0.1_0x01000100.ota` as a release asset.
3. The URL in `index.json` already points to the correct release asset path.

### Option B — Self-hosted (local network / dev)

Host `releases/ota-feed/` on any HTTP server and pass `--base-url`:

```powershell
tools/build_and_package_ota.ps1 -BaseUrl https://your-host/ota
```

Or serve the feed directory locally for quick testing:

```bash
cd firmware/releases/ota-feed
python -m http.server 8080
# base-url = http://<your-ip>:8080
```

---

## Step 4 — Configure Zigbee2MQTT to use the OTA index

In your `zigbee2mqtt/configuration.yaml`:

```yaml
ota:
  update_check_interval: 1440        # minutes between automatic checks
  disable_automatic_update_check: false
  zigbee_ota_override_index_location: https://raw.githubusercontent.com/muriloneo/kommando/main/firmware/releases/ota-feed/index.json
```

> For a local feed, use `zigbee_ota_override_index_location: /path/to/index.json` (absolute path).

Restart Zigbee2MQTT after saving.

---

## Step 5 — Trigger the OTA update

### Via Z2M frontend (recommended)

1. Open Zigbee2MQTT web UI → **OTA** tab.
2. Find `Kommando_Nano` in the device list.
3. Click **Check for update** — Z2M queries the OTA index and shows available version.
4. Click **Update** to start the transfer.

### Via MQTT (automation / CLI)

```bash
# Check for update
mosquitto_pub -t zigbee2mqtt/Kommando_Nano/ota_update/get -m ''

# Start update
mosquitto_pub -t zigbee2mqtt/Kommando_Nano/ota_update/update -m ''
```

Z2M publishes progress on:
```
zigbee2mqtt/Kommando_Nano   {"update": {"state": "updating", "progress": 42, "remaining": 58}}
```

---

## Step 6 — What to expect on the device

| Phase | Serial log | Duration |
|-------|-----------|---------|
| Discovery | `OTA server discovered` | ~8 s after join |
| Query | `OTA image found: version=0x...` | instantaneous |
| Transfer | `OTA receive progress [x/y]` | ~3–8 min per 1 MB |
| Validation | `OTA check status: OK` | instantaneous |
| Apply | `OTA complete, restarting...` | device reboots |
| Done | Device rejoins with new firmware | ~10 s |

The device will automatically reboot when the transfer completes. It rejoins the network
running the new firmware version.

---

## OTA acceptance rules

The device firmware rejects an update if **any** of these conditions fail:

- `manufacturerCode` does not match `0x1234`
- `imageType` does not match Kconfig `CONFIG_KOMMANDO_OTA_IMAGE_TYPE` (default `0x1011`)
- `fileVersion` is not strictly greater than the running version

This means sending the same version again does nothing (by design).

---

## Forcing an update (downgrade or same version)

To force update to the same or lower version, set `"force": true` in the index entry
**and** trigger the update via MQTT:

```bash
mosquitto_pub -t zigbee2mqtt/Kommando_Nano/ota_update/update -m '{"force": true}'
```

---

## Kconfig OTA parameters (via `idf.py menuconfig`)

| Config key | Default | Description |
|------------|---------|-------------|
| `CONFIG_KOMMANDO_OTA_IMAGE_TYPE` | `0x1011` | Must match OTA image and Z2M index |
| `CONFIG_KOMMANDO_OTA_HW_VERSION` | `0x0101` | Hardware version for optional server filtering |
| `CONFIG_KOMMANDO_OTA_QUERY_INTERVAL_MIN` | `60` | Minutes between periodic image queries |
| `CONFIG_KOMMANDO_OTA_MAX_DATA_SIZE` | `223` | Block size per OTA transfer chunk (max 223) |

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Z2M shows "no update available" | `index.json` not loaded or URL unreachable | Verify `zigbee_ota_override_index_location` and URL |
| Z2M shows update but device ignores it | `manufacturerCode` or `imageType` mismatch | Regenerate `.ota` matching sdkconfig values |
| Update starts but stalls | Coordinator/device RF issue | Move device closer; retry |
| Device reboots but version unchanged | `esp_ota_set_boot_partition` failed | Check serial log for `OTA apply` errors |
| "No OTA partition available" in logs | Partition table missing `ota_0`/`ota_1` | Flash the correct partition table (`partition_table/partition-table.csv`) |
| Z2M reports `fileVersion` mismatch | New image version ≤ running version | Bump `FW_VERSION_*` in `config.h` and rebuild |

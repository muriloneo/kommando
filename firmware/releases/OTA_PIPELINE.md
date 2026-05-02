# Kommando OTA Build & Publish Pipeline

Document version: 1.0.0
Last updated: 2026-03-29

This guide documents the scripted flow to:

1. build firmware,
2. generate Zigbee OTA image (`.ota`),
3. publish a versioned artifact into OTA feed folder,
4. update index metadata for Z2M hosting.

---

## Files added

- `tools/build_and_package_ota.py` (main pipeline)
- `tools/build_and_package_ota.ps1` (PowerShell wrapper)
- `releases/ota-feed/index.json` (OTA feed metadata)
- `.gitignore` rules for OTA artifact allowlist behavior

---

## Behavior guarantees

### 1) Versioned artifacts

Published OTA files are copied into feed as:

- `kommando_<PROJECT_VER>_0x<FILE_VERSION>.ota`

Example:

- `kommando_1.0.1_0x01000100.ota`

A sidecar metadata file is also generated:

- `kommando_1.0.1_0x01000100.ota.json`

### 2) Ignore same target/version

The pipeline checks existing feed `index.json` for duplicate tuple:

- `manufacturerCode`
- `imageType`
- `fileVersion`

If already present, publish is ignored as a **graceful no-op** (exit code `0`).

### 3) Git ignore allowlist behavior

OTA artifacts are ignored by default, but OTA files inside `releases/ota-feed/` are explicitly allowlisted via `.gitignore` patterns.

This keeps random local OTA files out of git while allowing pushable OTA feed artifacts.

### 4) Graceful failure

Any step failure (configure/build/ota generation/publish) exits with a clear error message and non-zero code.
No half-written index entries are produced for failed publishes.

---

## Prerequisites

- ESP-IDF environment active (same one you use for normal build)
- Python with `zigpy` installed:
  - `tools/requirements-ota.txt`

---

## Run from PowerShell

From firmware root:

- GitHub Releases mode (recommended):
  - `tools/build_and_package_ota.ps1 -GitHubRepo muriloneo/kommando -GitHubTag v1.0.2`

- Self-hosted feed mode:
  - `tools/build_and_package_ota.ps1 -BaseUrl https://your-host/ota`

Optional:

- `tools/build_and_package_ota.ps1 -Configure ...`
- `tools/build_and_package_ota.ps1 -SkipBuild ...`

---

## Run Python script directly

- GitHub Releases mode (recommended):
  - `python tools/build_and_package_ota.py --project-dir . --github-repo muriloneo/kommando --github-tag v1.0.2`

- Self-hosted feed mode:
  - `python tools/build_and_package_ota.py --project-dir . --base-url https://your-host/ota`

Optional:

- `--configure`
- `--skip-build`

---

## Feed artifacts

After successful publish:

- `.ota` copied to `releases/ota-feed/`
- `index.json` updated with entry
- per-release sidecar JSON created

---

## Device-side compatibility checks (already in firmware)

Device OTA client accepts update only when:

- manufacturer code matches
- image type matches
- file version is newer than running version

So same-version OTA is ignored device-side as well.

---

## GitHub Releases pipeline

A ready-to-use CI workflow is provided at:

- `.github/workflows/ota-release.yml`

Behavior:

1. Builds firmware + Zigbee OTA image on tag push (`v*`) or manual dispatch.
2. Runs OTA packaging script with `--github-repo` and `--github-tag`.
3. Produces `index.json` entries pointing to release asset URLs:
   - `https://github.com/<owner>/<repo>/releases/download/<tag>/<file>.ota`
4. Uploads OTA artifacts (and index) to GitHub Release.

This removes the need to manually provide a custom base URL when using GitHub Releases.

# Kommando Firmware — Workspace Setup

This directory contains the **Kommando Nano** ESP32-C6 Zigbee touchscreen wall panel firmware.

## Repository Structure

```
kommando/                          ← Repository root (contains .git/)
├── firmware/                       ← THIS DIRECTORY (VS Code workspace)
│   ├── main/                       ← Application source code
│   ├── tools/                      ← Build and OTA pipeline tools
│   ├── partition_table/            ← OTA partition definitions
│   ├── managed_components/         ← IDF component dependencies
│   ├── releases/                   ← OTA artifacts and changelog
│   ├── .github/                    ← Firmware CI/CD hooks and Copilot instructions
│   ├── .vscode/                    ← VS Code workspace settings
│   └── .repomixignore              ← Configuration for AI snapshot generation
├── z2m-converter/                  ← Zigbee2MQTT external converter
├── home-assistant-blueprint/       ← Home Assistant automation blueprint
├── ki-kad/                         ← KiCad PCB design
├── .github/                        ← Root monorepo configuration
└── .git/                           ← Git repository root
```

## Opening in VS Code

**Recommended approach**: Open the `firmware/` folder as your workspace.

```bash
git clone https://github.com/muriloneo/kommando.git
cd kommando/firmware
code .
```

When you open `firmware/` as the workspace:
- Git operations work correctly (repository root is found at parent level)
- Copilot reads instructions from `firmware/.github/copilot-instructions.md`
- All relative paths (main/, tools/, .ai/) are resolved correctly
- VS Code terminal starts in the firmware/ directory

## AI Context Setup

### Generate Repository Snapshot

Before asking Copilot for help, generate a compressed repository snapshot:

```bash
./tools/ai-context.sh
```

This creates:
- **`.ai/repo.xml`** (219 KB) — Structured XML for AI parsing
- **`repo-context.md`** (219 KB) — Markdown for Copilot chat

Both are regenerated each time and **not committed** to git (see `.gitignore`).

### Use with Copilot

In Copilot chat:
1. Drag and drop `repo-context.md` into the chat, OR
2. Reference specific files you're working on, OR
3. Use skill prompts from `.ai/skills/`:
   - `@debug.md` — diagnose build/runtime issues
   - `@feature.md` — design new features safely  
   - `@refactor.md` — refactor with measurable justification

### RTK Token Savings

All `repomix` commands are tracked via RTK to measure token efficiency:

```bash
rtk gain              # Show savings dashboard
rtk gain --history    # Per-command savings
```

Expected: **97-99% token savings** on large repo snapshots.

## Development Workflow

### Build and Flash

```bash
# Build firmware
idf.py build

# Build and flash to serial port
idf.py -p /dev/ttyUSB0 flash monitor

# OTA packaging (for release)
./tools/build_and_package_ota.py --github-repo muriloneo/kommando --github-tag v1.0.0
```

### Git Workflow

Standard monorepo git workflow:

```bash
# Check firmware-specific changes
cd kommando/firmware
rtk git status
rtk git diff main/config.h

# Commit changes (affects whole repo)
git add firmware/main/config.h
git commit -m "firmware: update pin mappings"

# Push to main repo
git push origin main
```

## Monorepo Notes

- The repository root (`.git`) is at the **parent level** (`kommando/`)
- Opening `firmware/` as the workspace keeps it organized and isolated for VS Code
- Git commands automatically discover the correct repository root
- CI/CD workflows (`.github/workflows/`) trigger on changes to the `firmware/` directory

## Documentation

- [Kommando main README](../README.md)
- [OTA Update Pipeline](./releases/OTA_PIPELINE.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/)
- [Home Assistant Integration](../home-assistant-blueprint/)
- [Zigbee2MQTT Converter](../z2m-converter/)

## License

This firmware is part of the Kommando project, licensed under **GNU GPL v3.0**.

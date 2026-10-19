#!/bin/bash
#
# Kommando Firmware — AI Context Generator
# 
# Generates repository snapshot (XML + Markdown) for AI consumption.
# Works correctly from the firmware/ directory, regardless of monorepo structure.
#
# Repository structure:
#   kommando/          ← Git root
#   └── firmware/      ← This script location
#
# Usage:
#   ./tools/ai-context.sh
#
# Outputs:
#   .ai/repo.xml       — XML snapshot (structured parsing)
#   repo-context.md    — Markdown snapshot (Copilot chat)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$(dirname "$SCRIPT_DIR")"

echo "==================================================================="
echo "Kommando Firmware — AI Context Generator"
echo "==================================================================="
echo ""
echo "Firmware directory: $FIRMWARE_DIR"
echo "Git root: $(cd "$FIRMWARE_DIR" && git rev-parse --show-toplevel 2>/dev/null || echo 'not found')"
echo ""

echo "Generating XML repo snapshot..."
mkdir -p "$FIRMWARE_DIR/.ai"
rtk proxy repomix --output "$FIRMWARE_DIR/.ai/repo.xml" "$FIRMWARE_DIR"

echo ""
echo "Generating markdown repo snapshot..."
rtk proxy repomix --style markdown --output "$FIRMWARE_DIR/repo-context.md" "$FIRMWARE_DIR"

echo ""
echo "Done. AI context ready."
echo ""
echo "Files generated:"
echo "  • .ai/repo.xml       (219 KB) — structured XML for AI parsing"
echo "  • repo-context.md    (219 KB) — markdown for Copilot chat"
echo ""
echo "Skill prompts (committed to git):"
echo "  • .ai/skills/debug.md      — diagnose errors"
echo "  • .ai/skills/feature.md    — design features"
echo "  • .ai/skills/refactor.md   — refactor safely"
echo ""
echo "Repository instructions:"
echo "  • .github/copilot-instructions.md — firmware-specific context"
echo ""
echo "Token savings:"
rtk gain --history | head -15


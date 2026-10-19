# Memory Management Protocol

Hermes uses a three-tier memory system. This document describes how it works.

## Tiers

| Tier | Store | Size | Load Behavior |
|---|---|---|---|
| **Hot** | MEMORY.md | 2,200 chars | Injected into every session automatically |
| **Warm** | `memory-bank` skill | Unlimited | Loaded on-demand via `skill_view(name='memory-bank')` |
| **Cold** | Obsidian vault notes | Unlimited | Manual read when deep context needed |

## Swap Protocol

### Hot → Warm (demote)
When MEMORY.md is full and a new entry needs to go in:
1. Identify the least-recently-used entry in MEMORY.md
2. Move it to the `memory-bank` skill under the appropriate section
3. Add the new entry to MEMORY.md via `memory(action='replace')`

### Warm → Hot (promote)
When a frequently-needed context should be in hot memory:
1. `skill_view(name='memory-bank')` to read the warm tier
2. Use `memory(action='replace')` to swap an entry in MEMORY.md

### Cold reads
When you need deep context not in hot or warm:
1. Read the relevant Obsidian note directly
2. If it becomes frequently needed, promote to warm skill

## Key Files
- `~/.hermes/memories/MEMORY.md` — hot tier (2,200 chars)
- `~/.hermes/memories/USER.md` — user profile (1,375 chars)
- `~/.hermes/skills/memory-bank/SKILL.md` — warm tier (unlimited)
- `/opt/data/vault/Infrastructure/Memory/` — cold tier notes

## When to Load memory-bank
- Task needs context not in MEMORY.md
- User references a project/topic previously discussed
- Need hardware details, build instructions, or past decisions
- Kommando, CH32V208, ESP-IDF, or other project-specific context

## Imported ChatGPT Sessions
- Kommando Nano: session `chatgpt_esp32_import_84caf6ab` (296 msgs)
- HW Context Review: session `chatgpt_hw_import_128d0d81` (20 msgs)
- Both browsable in Hermes sidebar

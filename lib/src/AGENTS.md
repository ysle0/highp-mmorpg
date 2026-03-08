<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/src

## Purpose
Implementation files for lib headers. Only headers that cannot be fully defined inline (non-template, non-constexpr code) have a corresponding `.cpp` here.

## Key Files
| File | Description |
|------|-------------|
| (none at root) | All implementations are inside domain subdirectories |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| logger/ | TextLogger implementation (file and console output) |
| scope/ | Defer utility implementation |

## For AI Agents
### Working In This Directory
- Every `.cpp` here corresponds to a header in `lib/inc/`. Keep the subdirectory structure mirrored.
- The precompiled header (`pch.h`) is injected automatically by the project build settings - do not add `#include "pch.h"` manually unless the project requires it explicitly.
- Template and constexpr definitions belong in `.hpp` files under `lib/inc/`, not here.

### Common Patterns
- Implementation files include their matching header first, then any other dependencies.
- Avoid including headers from `network/` or `exec/` - `lib/src/` must remain dependency-free of upper layers.

## Dependencies
### Internal
- `lib/inc/` headers only.

### External
- Windows SDK (where required).
- C++17/20 standard library.

<!-- MANUAL: -->

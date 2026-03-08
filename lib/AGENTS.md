<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib

## Purpose
Static library providing shared utilities used across the entire project - Result type, logging, error handling, memory pools, concurrency primitives, and TOML config parsing. All symbols are namespaced under `highp::fn`, `highp::log`, `highp::err`, `highp::concurrency`, or `highp::scope`.

## Key Files
| File | Description |
|------|-------------|
| lib.cpp | Library entry point / precompiled header trigger |
| pch.h | Precompiled header declaration |
| pch.cpp | Precompiled header compilation unit |
| framework.h | Common Windows/SDK includes and forward declarations |
| lib.vcxproj | Visual Studio project file for the static library |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| inc/ | Public headers organized by domain (concurrency, config, error, functional, logger, macro, memory, scope) |
| src/ | Implementation files for headers that require a .cpp compilation unit |

## For AI Agents
### Working In This Directory
- This is a static library; it produces no executable. Changes here affect all consumers (network/, exec/).
- Headers live in `inc/` and are the public API. Implementations live in `src/`.
- Precompiled header (`pch.h`) is included first in every `.cpp` via project settings - do not add it manually inside `.cpp` files.
- After editing `.toml` config files, re-run the appropriate `scripts/parse_*.ps1` script to regenerate the derived C++ header.

### Common Patterns
- Use `Result<T, E>` (from `inc/functional/Result.hpp`) for fallible operations instead of exceptions or raw error codes.
- Use the `GUARD` macro to propagate errors early without verbose if-chains.
- Prefer `HybridObjectPool` for high-frequency allocations on hot paths (e.g., overlapped I/O contexts).
- All log calls go through the `Logger` template; choose verbosity via `ELoggerVerbosity`.

## Dependencies
### Internal
- None (this is the lowest-level library; nothing inside `lib/` depends on `network/` or `exec/`).

### External
- Windows SDK (for HANDLE, CRITICAL_SECTION, etc. used in memory/concurrency headers)
- C++17/20 standard library (std::optional, std::variant, std::atomic, std::mutex, etc.)

<!-- MANUAL: -->

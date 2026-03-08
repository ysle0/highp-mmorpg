<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc

## Purpose
Public header files for the lib utilities, organized by domain. All headers here form the public API of the static library and are safe to include from any consumer project (network/, exec/).

## Key Files
| File | Description |
|------|-------------|
| (none at root) | All headers are inside domain subdirectories |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| concurrency/ | MPSC queue and other thread-safe concurrent data structures |
| config/ | TOML configuration parsers (compile-time and runtime variants) |
| error/ | Error type definitions used with Result<T,E> |
| functional/ | Result<T,E> type and functional programming utilities |
| logger/ | Logging system interface, implementation headers, and verbosity enum |
| macro/ | Utility macros shared across the codebase |
| memory/ | Object pool implementations for high-performance memory reuse |
| scope/ | RAII scope guard and deferred execution utilities |

## For AI Agents
### Working In This Directory
- Do not place headers directly in `lib/inc/`. All headers belong inside a domain subdirectory.
- Header-only implementations use `.hpp`; headers that require a `.cpp` compilation unit use `.h`.
- Include guards or `#pragma once` must be present in every file.
- When adding a new domain, create a matching subdirectory in `lib/src/` only if an implementation `.cpp` is needed.

### Common Patterns
- Domain headers are included transitively; prefer including the narrowest header needed rather than a catch-all.
- Template and constexpr utilities are fully defined in `.hpp` files (no separate `.cpp`).

## Dependencies
### Internal
- Headers within the same `inc/` tree may include siblings (e.g., `functional/Result.hpp` may be included by `error/` consumers).

### External
- Windows SDK headers where required (pulled in via `lib/framework.h` or `pch.h`).
- C++17/20 standard library headers.

<!-- MANUAL: -->

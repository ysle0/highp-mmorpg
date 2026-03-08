<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# exec

## Purpose
Container for all runnable server and client executables. Each subdirectory is a separate VS project targeting different protocol demonstrations built on top of the shared `network/` static library.

## Key Files
| File | Description |
|------|-------------|

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| echo/ | Echo server/client pair - simplest IOCP networking stack demo |
| chat/ | Chat server/client pair - FlatBuffers protocol and room-based messaging |
| playground/ | Experimental code for testing ideas outside the main architecture |

## For AI Agents
### Working In This Directory
- Each subdirectory is an independent Visual Studio project (`.vcxproj`).
- All executables link against `network/` (static lib) and `lib/` (utilities).
- Do not add shared source files here; place them in `network/` or `lib/` instead.

### Common Patterns
- Entry points live in `main.cpp` inside each project subdirectory.
- Runtime configuration is loaded from `config.runtime.toml` at startup.
- Business logic classes implement `ISessionEventReceiver` from the network library.

## Dependencies
### Internal
- `network/` - IOCP, sockets, async I/O primitives (static library)
- `lib/` - Result type, logging, error handling utilities

### External
- Windows SDK (IOCP / Winsock2)
- Visual Studio 2022 / MSBuild (build toolchain)

<!-- MANUAL: -->

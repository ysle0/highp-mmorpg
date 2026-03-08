<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/config

## Purpose
Auto-generated configuration headers providing compile-time constants and runtime network configuration. These files are produced by PowerShell scripts from TOML sources — do not edit them directly.

## Key Files
| File | Description |
|------|-------------|
| Const.h | Compile-time constants from config.compile.toml — recv/send buffer sizes and other fixed limits |
| NetworkCfg.h | Runtime config from config.runtime.toml — server port, max clients, worker thread counts; supports env var overrides (SERVER_PORT, SERVER_MAX_CLIENTS) |
| ConfigEmbedded.h | Embedded config utilities — helpers for accessing config values at runtime |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- **Do NOT edit these files directly.** Changes will be overwritten on the next script run.
- To change compile-time constants: edit `network/config.compile.toml`, then run `scripts/parse_compile_cfg.ps1`.
- To change runtime config: edit `echo/echo-server/config.runtime.toml` (or the chat-server equivalent), then run `scripts/parse_network_cfg.ps1`.
- Env var overrides (`SERVER_PORT`, `SERVER_MAX_CLIENTS`) are parsed at server startup and take precedence over TOML values.

### Common Patterns
- Buffer sizes from `Const.h` (e.g., recv buffer = 4096, send buffer = 1024) are used in `OverlappedExt` and `Client` to size stack/heap buffers.
- `NetworkCfg.h` values are read once at startup in `ServerLifecycle`; cache them rather than re-reading.

## Dependencies
### Internal
- Generated from: `network/config.compile.toml`, `echo/echo-server/config.runtime.toml`
- Consumed by: `network/inc/client/windows/Client.h`, `network/inc/server/ServerLifecycle.h`

### External
- None — pure C++ constants/structs, no external headers required

<!-- MANUAL: -->

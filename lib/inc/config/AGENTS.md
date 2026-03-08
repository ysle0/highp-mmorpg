<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/config

## Purpose
TOML configuration file parsers. Two variants are provided: compile-time (constexpr) for embedding config values as constants, and runtime for loading config from files at server startup with optional environment variable overrides.

## Key Files
| File | Description |
|------|-------------|
| (none at root) | All parsers live in the variant subdirectories |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| compileTime/ | Constexpr TOML parser for compile-time constant embedding |
| runTime/ | Runtime TOML parser with env-var override support |

## For AI Agents
### Working In This Directory
- Choose the correct variant based on the use case: `compileTime/` for values baked into the binary (buffer sizes, limits), `runTime/` for values read from disk at startup (port, thread counts).
- After modifying a `.toml` source file, regenerate the derived C++ header by running the appropriate script from the project root:
  - `powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1`
  - `powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1`
- Do not hand-edit generated headers; they are overwritten by the scripts.

### Common Patterns
- Compile-time config feeds into `network/inc/config/Const.h`.
- Runtime config feeds into `network/inc/config/NetworkCfg.h`.
- Environment variables (`SERVER_PORT`, `SERVER_MAX_CLIENTS`) override runtime TOML values when set.

## Dependencies
### Internal
- None within `lib/inc/config/`.

### External
- C++17 standard library (`<string_view>`, `<array>`, `<optional>`, etc.).
- No third-party TOML library; parsing is implemented directly in these headers.

<!-- MANUAL: -->

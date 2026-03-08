<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# scripts

## Purpose
Code generation scripts that translate source configuration and schema files into C++ headers. Each script ships as a `.ps1` (Windows/PowerShell) and `.sh` (Linux/CI bash) pair so the same generation step works in all environments.

## Key Files
| File | Description |
|------|-------------|
| parse_compile_cfg.ps1 | Reads `network/config.compile.toml` → writes `network/inc/config/Const.h` (compile-time buffer sizes and constants) |
| parse_compile_cfg.sh | Bash equivalent of `parse_compile_cfg.ps1` for Linux/CI |
| parse_network_cfg.ps1 | Reads `echo/echo-server/config.runtime.toml` → writes `network/inc/config/NetworkCfg.h` (runtime server config with env-var override support) |
| parse_network_cfg.sh | Bash equivalent of `parse_network_cfg.ps1` for Linux/CI |
| gen_flatbuffers.ps1 | Invokes the `flatc` compiler on all `.fbs` schemas under `protocol/flatbuf/schemas/` → writes `protocol/flatbuf/gen/*_generated.h` |
| gen_flatbuffers.sh | Bash equivalent of `gen_flatbuffers.ps1` for Linux/CI |

## Subdirectories
None.

## For AI Agents
### Working In This Directory
- Run the correct script whenever source files change:

  | Changed file | Script to run |
  |---|---|
  | `network/config.compile.toml` | `parse_compile_cfg.ps1` / `.sh` |
  | `echo/echo-server/config.runtime.toml` | `parse_network_cfg.ps1` / `.sh` |
  | Any `protocol/flatbuf/schemas/**/*.fbs` | `gen_flatbuffers.ps1` / `.sh` |

- On Windows:
  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts/<script>.ps1
  ```
- On Linux/CI:
  ```bash
  bash scripts/<script>.sh
  ```
- Generated headers (`Const.h`, `NetworkCfg.h`, `*_generated.h`) should be committed to source control so the project builds without requiring external tools to be installed on every machine.

### Common Patterns
- Scripts are idempotent: running them multiple times with unchanged inputs produces identical output.
- `.ps1` and `.sh` pairs implement the same logic; keep them in sync when modifying generation behavior.
- `flatc` must be on the system PATH for `gen_flatbuffers.*` to succeed.

## Dependencies
### Internal
- `network/config.compile.toml` - source for `parse_compile_cfg.*`
- `echo/echo-server/config.runtime.toml` - source for `parse_network_cfg.*`
- `protocol/flatbuf/schemas/` - source `.fbs` files for `gen_flatbuffers.*`

### External
- PowerShell 5.1+ (Windows) or bash (Linux/CI)
- FlatBuffers `flatc` compiler (required only for `gen_flatbuffers.*`)

<!-- MANUAL: -->

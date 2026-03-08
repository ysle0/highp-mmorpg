<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/config/runTime

## Purpose
Runtime TOML parser for loading configuration from files at server startup. Supports environment variable overrides so deployment environments can inject values without recompiling or editing TOML files.

## Key Files
| File | Description |
|------|-------------|
| Config.hpp | Main config interface; loads a TOML file at startup and exposes typed accessors with env-var override logic |
| StringUtils.hpp | String utilities for runtime parsing (trim, split, type conversion helpers) |
| TomlEntry.hpp | TOML key-value entry type for runtime use; stores key and value as std::string |
| TomlParser.hpp | Runtime TOML parser; reads a file path, parses key=value lines, returns a collection of TomlEntry |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- Runtime parsers may use heap allocation, file I/O, and the full standard library - no constexpr restriction.
- Environment variable override precedence: env var > TOML file value > compiled default. Preserve this order when modifying `Config.hpp`.
- Supported env vars: `SERVER_PORT`, `SERVER_MAX_CLIENTS`. Adding a new override requires updating both `Config.hpp` and the relevant TOML file comment.
- After changes, re-run `scripts/parse_network_cfg.ps1` and confirm `network/inc/config/NetworkCfg.h` reflects the new structure.

### Common Patterns
- `Config` is typically instantiated once at server startup and its values copied into a plain struct passed to components.
- Missing TOML keys fall back to hardcoded defaults in `Config.hpp`; do not silently ignore missing keys without providing a sensible default.

## Dependencies
### Internal
- `StringUtils.hpp` is used by `TomlParser.hpp`.

### External
- C++17 `<string>`, `<fstream>`, `<optional>`, `<unordered_map>`, `<cstdlib>` (for `std::getenv`).

<!-- MANUAL: -->

<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/config/compileTime

## Purpose
Compile-time (constexpr) TOML parser for embedding configuration values as constants directly in the binary. Values parsed here are available as `constexpr` expressions with zero runtime overhead.

## Key Files
| File | Description |
|------|-------------|
| Config.hpp | Main config interface; exposes the constexpr API for querying TOML values at compile time |
| StringUtils.hpp | String utilities (constexpr string_view helpers) used internally by the parser |
| TomlEntry.hpp | TOML key-value entry type; represents a single parsed key/value pair as a constexpr struct |
| TomlParser.hpp | Core constexpr TOML parser; reads a string literal and produces an array of TomlEntry values |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- All code here must remain `constexpr`-compatible. Do not introduce heap allocation, virtual dispatch, or non-constexpr standard library calls.
- The typical usage pattern is: `TomlParser` ingests a raw string literal, produces `TomlEntry` array, `Config` provides typed accessors.
- Changes here affect the generated `network/inc/config/Const.h` header. After editing, re-run `scripts/parse_compile_cfg.ps1` and verify the generated header is correct.
- TOML subset supported is intentionally minimal (flat key = value, string and integer literals). Do not expand the supported grammar without coordinating with the scripts.

### Common Patterns
- Consumers pass a `constexpr` string literal (the TOML file content embedded via a raw string or `#embed`) to `TomlParser`, then query by key through `Config`.
- Error cases at compile time manifest as `static_assert` failures, making misconfiguration visible at build time.

## Dependencies
### Internal
- `StringUtils.hpp` is used by `TomlParser.hpp` and `TomlEntry.hpp`.

### External
- C++17 `<string_view>`, `<array>`, `<cstddef>` (all constexpr-capable).

<!-- MANUAL: -->

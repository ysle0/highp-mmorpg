<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/src/scope

## Purpose
Scope guard implementation. Contains the compilation unit for `Defer`, providing the non-inline portions of the deferred execution utility.

## Key Files
| File | Description |
|------|-------------|
| Defer.cpp | `Defer` utility implementation; defines any out-of-line methods for the `Defer` class declared in `lib/inc/scope/Defer.h` |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- `Defer.cpp` implements the non-template, non-inline parts of `Defer.h`. If `Defer` becomes fully inline/header-only, this file can be removed and the subdirectory collapsed.
- `DeferContext.hpp` and `DeferredItemHolder.hpp` are header-only and have no corresponding `.cpp` here.
- Keep changes here minimal; most scope-guard logic belongs in the header for inlining at the callsite.

### Common Patterns
- If adding a new method to `Defer` that must remain out-of-line (e.g., for binary size reasons), declare it in `Defer.h` and define it here.

## Dependencies
### Internal
- `lib/inc/scope/Defer.h`

### External
- C++17 standard library (`<utility>`, `<functional>` as needed by the Defer implementation).

<!-- MANUAL: -->

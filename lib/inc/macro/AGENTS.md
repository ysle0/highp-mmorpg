<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/macro

## Purpose
Utility macros shared across the codebase. Provides common preprocessor definitions that reduce boilerplate and enforce consistent patterns project-wide.

## Key Files
| File | Description |
|------|-------------|
| macro.h | Common utility macros (e.g., non-copyable/non-movable class helpers, unreachable annotations, platform guards) |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- Keep macros minimal and well-named. Prefer inline functions or templates over macros when the language supports it equally well.
- Every macro must have a comment explaining its purpose and expected usage at the definition site.
- Do not define macros that shadow standard library names or Windows SDK macros.
- `macro.h` is typically included via `pch.h` and is therefore available everywhere without explicit inclusion.

### Common Patterns
- Use `HIGHP_DISALLOW_COPY(ClassName)` (or equivalent) in classes that manage exclusive resources (sockets, IOCP handles) to prevent accidental copies.
- Use an `UNREACHABLE()` macro in switch default branches that should never be hit, to get a compiler warning/error if new enum values are added without handling.

## Dependencies
### Internal
- None.

### External
- Compiler intrinsics only (e.g., `__assume`, `__builtin_unreachable`) where used inside macros.

<!-- MANUAL: -->

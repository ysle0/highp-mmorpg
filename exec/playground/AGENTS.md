<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# playground

## Purpose
Experimental and scratch code for testing ideas outside the main server architecture. Not intended for production use; changes here have no stability guarantees.

## Key Files
| File | Description |
|------|-------------|
| playground.cpp | Experimental code - ad-hoc tests and prototypes |
| playground.vcxproj | Visual Studio project file |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- Code here is intentionally informal; normal code quality and review standards do not apply.
- If an experiment here proves useful, extract it into `network/` or `lib/` with proper structure before referencing it from other projects.
- Do not link playground against `echo-server` or `chat-server` targets; it should remain an isolated executable.

### Common Patterns
- Single `main()` in `playground.cpp`; add additional `.cpp` files as needed for larger experiments.
- Prefer `#include` of headers from `network/inc/` or `lib/inc/` rather than copying code in.

## Dependencies
### Internal
- `network/` - available to link against for prototyping
- `lib/` - available to link against for prototyping

### External
- Windows SDK (as needed per experiment)

<!-- MANUAL: -->

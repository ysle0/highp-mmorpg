<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/src/logger

## Purpose
Logger implementation. Contains the compilation unit for `TextLogger`, which provides formatted log output to console and/or file with thread-safe writes.

## Key Files
| File | Description |
|------|-------------|
| TextLogger.cpp | `TextLogger` implementation; formats log lines with timestamp, verbosity label, and message, then writes to the configured output targets under a mutex |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- `TextLogger.cpp` implements the interface declared in `lib/inc/logger/TextLogger.h`. Keep the two in sync.
- All output writes must be protected by the internal mutex to prevent interleaved log lines from concurrent IOCP worker threads.
- Timestamp format and verbosity label strings are defined here; if changing them, update any log-parsing tooling or documentation that depends on the format.
- Do not add file I/O outside of the logger initialization path; `TextLogger` should open the log file once at construction and close it at destruction.

### Common Patterns
- Format: `[TIMESTAMP] [LEVEL] message\n` - preserve this structure so log lines remain grep-friendly.
- Flush policy: flush on Error/Fatal level to ensure critical messages are not lost on crash; buffer lower-level output for performance.

## Dependencies
### Internal
- `lib/inc/logger/TextLogger.h`
- `lib/inc/logger/ILogger.h`
- `lib/inc/logger/ELoggerVerbosity.h`
- `lib/inc/logger/LoggerOptions.h`

### External
- C++17 `<fstream>`, `<mutex>`, `<string>`, `<chrono>` (for timestamps).

<!-- MANUAL: -->

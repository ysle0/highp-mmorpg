<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/logger

## Purpose
Logging system with verbosity control. Provides an interface-based design so the logging backend can be swapped (e.g., text file, console, structured JSON) without changing callsites.

## Key Files
| File | Description |
|------|-------------|
| Logger.hpp | Main `Logger<TImpl>` class template; wraps a concrete logger implementation and forwards calls with verbosity filtering |
| ILogger.h | Abstract logger interface (`ILogger`) defining the contract all backend implementations must satisfy |
| TextLogger.h | `TextLogger` concrete implementation header; writes formatted log lines to console and/or file |
| ELoggerVerbosity.h | `ELoggerVerbosity` enum defining log levels (e.g., Trace, Debug, Info, Warning, Error, Fatal) |
| LoggerOptions.h | `LoggerOptions` struct configuring a logger instance (output target, minimum verbosity, timestamp format, etc.) |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- `Logger.hpp` is the only type callsites should reference directly; do not couple code to `TextLogger` or `ILogger` in business logic.
- Log verbosity filtering happens in `Logger<TImpl>` before dispatching to the implementation; implementations can assume the verbosity check has already passed.
- When adding a new log level to `ELoggerVerbosity`, update both the enum and any switch statements in `TextLogger` that map levels to output strings.
- `LoggerOptions` controls runtime behavior (output path, min level). Pass it at construction time; do not mutate after construction.

### Common Patterns
- Instantiate once at startup, pass by reference or via a global accessor to components that need logging.
- Use the lowest sufficient verbosity level: Debug for diagnostic detail, Info for normal operational events, Warning for recoverable anomalies, Error/Fatal for failures.
- Log structured context (client ID, packet type) alongside the message to aid post-mortem debugging.

## Dependencies
### Internal
- `ELoggerVerbosity.h` is included by `ILogger.h`, `Logger.hpp`, and `TextLogger.h`.
- `LoggerOptions.h` is included by `TextLogger.h` and `Logger.hpp`.

### External
- C++17 `<string>`, `<fstream>`, `<mutex>` (for thread-safe output in `TextLogger`).

<!-- MANUAL: -->

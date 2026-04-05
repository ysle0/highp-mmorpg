<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/functional

## Purpose
Functional programming utilities. Provides `Result<T,E>`, a C++23 `std::expected`-style type for explicit error propagation without exceptions.

## Key Files
| File | Description |
|------|-------------|
| Result.hpp | `Result<T,E>` type with `Ok`/`Err` factory methods. Includes `void` specialization for operations that succeed without a return value. |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- `Result<T,E>` is the primary error-handling mechanism across the entire codebase. Prefer it over exceptions, `std::optional`, or raw error codes.
- `Ok(value)` constructs a success result; `Err(error)` constructs a failure result.
- `Result<void, E>` is the specialization for operations that succeed without returning data (e.g., `PostRecv`, `PostSend`).
- Propagate failures with explicit checks:
  ```cpp
  const Result<T, E> result = SomeFallibleCall();
  if (result.HasErr()) {
      return result;
  }
  ```
- Do not unwrap `Result` with `.value()` without first checking `.IsOk()`; treat it like `std::optional`.
- When adding methods to `Result`, ensure both the `T` and `void` specializations are updated consistently.

### Common Patterns
- Fallible functions return `Result<T, ESomeError>`.
- Chain of calls uses small explicit `if (result.HasErr())` checks to avoid deeply nested logic.
- At the top level (e.g., in a completion handler), match on the result and log the error before discarding it.

## Dependencies
### Internal
- None. `Result.hpp` is a foundational header with no dependencies on other `lib/inc/` headers.

### External
- C++17 `<variant>`, `<utility>`, `<type_traits>`.

<!-- MANUAL: -->

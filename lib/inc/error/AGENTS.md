<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/error

## Purpose
Error type definitions used with `Result<T,E>`. Centralizes all error enumerations so that callsites can pattern-match on specific failure modes without depending on concrete exception types.

## Key Files
| File | Description |
|------|-------------|
| Errors.hpp | Common error utilities shared across error domains (e.g., error-to-string helpers, base traits) |
| NetworkError.h | `ENetworkError` enum covering socket creation failures, IOCP registration errors, and AcceptEx failures |
| PacketError.h | `EPacketError` enum covering packet parse failures, dispatch failures, and verification failures |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- When adding a new error value to an existing enum, append to the end to avoid breaking existing switch exhaustiveness checks.
- Each enum value should have a clear, specific name that describes the failure site, not just "kFailed" or "kError".
- `ENetworkError` is used as the `E` type in `Result<T, ENetworkError>` throughout `network/`.
- `EPacketError` is used as the `E` type in `Result<T, EPacketError>` for packet processing paths.
- If a new error domain is needed (e.g., `EStorageError`), create a new `.h` file following the same enum pattern.

### Common Patterns
- Return errors via `Result<T,E>::Err(ENetworkError::kAcceptFailed)` rather than throwing or returning raw integers.
- Use explicit `if (result.HasErr())` checks with `Result<T,E>` to propagate errors up the call stack.

## Dependencies
### Internal
- `lib/inc/functional/Result.hpp` (consumers combine these error types with `Result<T,E>`).

### External
- No external dependencies; plain C++ enums.

<!-- MANUAL: -->

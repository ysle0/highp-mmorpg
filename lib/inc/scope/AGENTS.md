<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/scope

## Purpose
RAII scope guard and deferred execution utilities. Ensures cleanup actions run at scope exit regardless of how the scope is left, reducing the risk of resource leaks.

## Key Files
| File | Description |
|------|-------------|
| Defer.h | `Defer` class and `defer` macro for registering a callable to execute at scope exit (analogous to Go's `defer` statement) |
| DeferContext.hpp | `DeferContext` template; a typed scope guard that carries a context value alongside the cleanup action |
| DeferredItemHolder.hpp | Holds items (e.g., `AcceptOverlapped` contexts) for deferred cleanup; used by `IocpAcceptor` to manage AcceptOverlapped lifetime safely across async boundaries |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- `Defer.h` has a matching implementation in `lib/src/scope/Defer.cpp`; keep the header and implementation in sync.
- `DeferContext.hpp` and `DeferredItemHolder.hpp` are header-only.
- Use `defer { cleanup_code; };` (macro form) for simple one-shot cleanup. Use `DeferContext` when the cleanup action needs access to a typed value captured at the defer site.
- `DeferredItemHolder` is specifically tied to the IOCP acceptor pattern - do not repurpose it for unrelated lifetime management without understanding that coupling.
- Defer actions execute in LIFO order when multiple defers are registered in the same scope.

### Common Patterns
- Wrapping raw handle cleanup: `defer { CloseHandle(h); };` immediately after acquiring the handle.
- Guarding against early-return resource leaks in complex initialization sequences.
- `DeferredItemHolder` usage in `IocpAcceptor`: holds `AcceptOverlapped` objects alive until the async accept completion is delivered, then releases them.

## Dependencies
### Internal
- None within `lib/inc/scope/`.

### External
- C++17 standard library (`<utility>`, `<functional>` where lambdas are stored).

<!-- MANUAL: -->

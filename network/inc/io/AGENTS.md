<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/io

## Purpose
I/O multiplexing abstractions. Defines the `ICompletionTarget` interface that objects register as IOCP completion keys, and houses the Windows IOCP implementation under `windows/`.

## Key Files
| File | Description |
|------|-------------|
| CompletionTarget.hpp | ICompletionTarget interface — base for any object used as an IOCP completion key; completion handler receives CompletionEvent |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| windows/ | Windows IOCP multiplexer implementation (IocpIoMultiplexer) |

## For AI Agents
### Working In This Directory
- `ICompletionTarget` is the IOCP dispatch contract; `Client` and `IocpAcceptor` implement it.
- Implementations must be async-safe — completion callbacks are invoked from worker threads.
- Do not add platform-specific code here; put it in `windows/`.

### Common Patterns
- Objects associate themselves with IOCP via `IocpIoMultiplexer::AssociateSocket`, passing `this` as the completion key.
- On completion, `IocpIoMultiplexer` calls `completionTarget->OnCompletion(event)`.
- `CompletionTarget.hpp` uses `.hpp` extension because it contains the interface definition inline.

## Dependencies
### Internal
- `network/inc/client/windows/` — Client implements ICompletionTarget
- `network/inc/acceptor/` — IocpAcceptor implements ICompletionTarget

### External
- Windows SDK (via `windows/IocpIoMultiplexer.h`) — IOCP kernel object APIs

<!-- MANUAL: -->

<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/src/io/windows

## Purpose
IOCP multiplexer implementation. Creates the IOCP kernel object, spawns worker threads that block on GetQueuedCompletionStatus, and posts the shutdown sentinel via PostQueuedCompletionStatus.

## Key Files
| File | Description |
|------|-------------|
| IocpIoMultiplexer.cpp | CreateIoCompletionPort for handle creation and socket association; GQCS worker loop; PostQueuedCompletionStatus for shutdown signaling |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- Worker thread count is determined at `Initialize()` from `NetworkCfg.h`; threads are `std::jthread` and join automatically on destruction.
- Shutdown sequence: post one `PostQueuedCompletionStatus(handle, 0, NULL, NULL)` per worker thread; each thread exits on seeing a null completion key.
- `AssociateSocket` uses `CreateIoCompletionPort(socket, iocpHandle, completionKey, 0)` — the third argument is the `ICompletionTarget*` cast to `ULONG_PTR`.
- GQCS with `INFINITE` timeout is correct here; worker threads have no periodic work outside of completions.

### Common Patterns
- Worker loop: `GetQueuedCompletionStatus` -> null key check (shutdown) -> cast key to `ICompletionTarget*` -> `OnCompletion(event)`.
- `CompletionEvent` is stack-constructed in the worker loop from GQCS output and passed by value to `OnCompletion`.
- Zero bytes transferred with a non-null key indicates a graceful client disconnect — pass to `OnCompletion` so Client handles it.

## Dependencies
### Internal
- `network/inc/io/windows/IocpIoMultiplexer.h` — header being implemented
- `network/inc/io/CompletionTarget.hpp` — ICompletionTarget dispatched by worker threads
- `network/inc/config/NetworkCfg.h` — worker thread count

### External
- Windows SDK: `CreateIoCompletionPort`, `GetQueuedCompletionStatus`, `PostQueuedCompletionStatus`

<!-- MANUAL: -->

<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/io/windows

## Purpose
Windows IOCP I/O multiplexer implementation. Manages the IOCP kernel handle and worker thread pool, dispatching completion events to registered `ICompletionTarget` objects.

## Key Files
| File | Description |
|------|-------------|
| IocpIoMultiplexer.h | Manages IOCP kernel handle; worker thread pool via std::jthread; CompletionEvent struct; CompletionHandler callback type. Lifecycle: Initialize() -> SetCompletionHandler() -> AssociateSocket() -> Shutdown() |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- Worker threads block on `GetQueuedCompletionStatus` (GQCS); they are woken for I/O completions or a shutdown sentinel.
- Shutdown is signaled by posting a null completion key via `PostQueuedCompletionStatus` — one post per worker thread.
- `AssociateSocket` calls `CreateIoCompletionPort` to bind a socket to the IOCP handle with the `ICompletionTarget*` as the completion key.
- Thread count is read from `NetworkCfg.h` at `Initialize()` time.

### Common Patterns
- `CompletionEvent` wraps the GQCS output: transferred bytes, completion key (cast to `ICompletionTarget*`), and `OverlappedExt*`.
- Worker thread loop: GQCS -> check for shutdown sentinel -> cast key to `ICompletionTarget*` -> call `OnCompletion(event)`.
- `std::jthread` handles worker lifetime; destructor joins automatically on `Shutdown()`.

## Dependencies
### Internal
- `network/inc/io/CompletionTarget.hpp` — ICompletionTarget interface dispatched by worker threads
- `network/inc/config/NetworkCfg.h` — worker thread count

### External
- Windows SDK: `CreateIoCompletionPort`, `GetQueuedCompletionStatus`, `PostQueuedCompletionStatus`

<!-- MANUAL: -->

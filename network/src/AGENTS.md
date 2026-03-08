<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/src

## Purpose
Implementation files for network library headers. Each subdirectory mirrors the structure of `inc/` and contains the `.cpp` definitions for the corresponding public headers.

## Key Files
| File | Description |
|------|-------------|
| ServerLifecycle.cpp | Orchestrates IOCP + Acceptor + client pool; handles completion events and routes recv/send |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| acceptor/ | IocpAcceptor implementation |
| client/ | FrameBuffer, PacketStream, TcpClientSocket, WsaSession implementations |
| io/ | IOCP multiplexer implementation (windows/) |
| server/ | PacketDispatcher implementation |
| socket/ | AsyncSocket, SocketHelper, SocketOptionBuilder, WindowsAsyncSocket implementations |

## For AI Agents
### Working In This Directory
- Each `.cpp` file has a matching header in `inc/`; keep them in sync.
- Precompiled header `pch.h` is included first in every `.cpp` via project settings.
- Platform-specific implementations live under `windows/` subdirectories.

### Common Patterns
- IOCP worker loops use `GetQueuedCompletionStatus` (GQCS) and dispatch via `ICompletionTarget`.
- `PostQueuedCompletionStatus` with a null key signals graceful shutdown to worker threads.
- Error handling uses `Result<T, E>` — propagate rather than throw.

## Dependencies
### Internal
- `network/inc/` — all public headers this src tree implements
- `lib/inc/` — logger, functional, error, memory utilities

### External
- Windows SDK: `ws2_32.lib`, `mswsock.lib`

<!-- MANUAL: -->

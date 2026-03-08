<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/client/windows

## Purpose
Windows IOCP-specific client implementation. Provides the per-connection `Client` class with overlapped I/O contexts, the `EIoType` enum for distinguishing completion types, and `OverlappedExt` which extends WSAOVERLAPPED with operation metadata.

## Key Files
| File | Description |
|------|-------------|
| Client.h | Per-connection state: holds recv/send OverlappedExt, PostRecv/PostSend methods, inherits enable_shared_from_this and ICompletionTarget |
| EIoType.h | Enum for I/O operation types: Accept, Recv, Send — used to dispatch completions in the IOCP worker |
| OverlappedExt.h | WSAOVERLAPPED extension — adds IoType and owning Client pointer; CRITICAL: overlapped field MUST be first member |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- **CRITICAL INVARIANT**: `OverlappedExt.overlapped` (WSAOVERLAPPED) must remain the first struct member. The IOCP worker does `reinterpret_cast<OverlappedExt*>(lpOverlapped)` — any reordering breaks this cast silently.
- `Client` inherits `enable_shared_from_this` so completion callbacks can safely extend lifetime via `shared_from_this()`.
- `PostRecv` and `PostSend` both post overlapped operations; their OverlappedExt instances are separate to allow simultaneous in-flight recv and send.

### Common Patterns
- IOCP worker receives `OVERLAPPED*`, casts to `OverlappedExt*`, reads `IoType` to branch on Accept/Recv/Send.
- `Client::Close` uses SO_LINGER to control graceful vs. abortive close.
- `Client` is always heap-allocated and managed by `std::shared_ptr`; never stack-allocate.

## Dependencies
### Internal
- `network/inc/io/` — ICompletionTarget interface that Client implements
- `network/inc/client/` — FrameBuffer, PacketStream for recv data processing

### External
- Windows SDK: `winsock2.h`, WSARecv, WSASend, WSAOVERLAPPED

<!-- MANUAL: -->

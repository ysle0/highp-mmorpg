<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/src/client/windows

## Purpose
Windows IOCP Client implementation. Contains PostRecv/PostSend using WSARecv/WSASend with overlapped I/O, and Close with linger control.

## Key Files
| File | Description |
|------|-------------|
| Client.cpp | PostRecv/PostSend using WSARecv/WSASend overlapped I/O; Close with SO_LINGER control; OnCompletion dispatch by EIoType |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- `PostRecv` and `PostSend` must use the respective `OverlappedExt` instances (one for recv, one for send) — never reuse the same overlapped for both directions simultaneously.
- `WSASend` may complete synchronously (returns 0); handle both the synchronous and async (WSA_IO_PENDING) paths.
- `Close` with `SO_LINGER` (timeout=0) causes an abortive reset (RST); use a non-zero linger or `shutdown` first for graceful close.
- `shared_from_this()` is safe to call from `OnCompletion` because `Client` inherits `enable_shared_from_this` and is always managed by `shared_ptr`.

### Common Patterns
- `PostRecv` posts a single `WSARecv` with the recv `OverlappedExt`; on completion, bytes transferred are written into `FrameBuffer`.
- `PostSend` posts `WSASend` with the send `OverlappedExt`; on completion, notify `ISessionEventReceiver::OnSend`.
- On `ERROR_NETNAME_DELETED` or `ERROR_CONNECTION_ABORTED` in `OnCompletion`, call `ISessionEventReceiver::OnDisconnect` and do not re-post.

## Dependencies
### Internal
- `network/inc/client/windows/Client.h` — header being implemented
- `network/inc/client/FrameBuffer.h` — populated on recv completion
- `network/inc/server/ISessionEventReceiver.h` — callbacks for accept/recv/send/disconnect events

### External
- Windows SDK: WSARecv, WSASend, `ws2_32.lib`

<!-- MANUAL: -->

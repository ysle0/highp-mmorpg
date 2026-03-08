<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/src/acceptor

## Purpose
Implementation of `IocpAcceptor`. Contains the AcceptEx function pointer loading, overlapped accept posting, and completion handling logic.

## Key Files
| File | Description |
|------|-------------|
| IocpAcceptor.cpp | AcceptEx-based async acceptor implementation — WSAIoctl function pointer loading, PostAccept, OnCompletion dispatch |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- After each successful accept completion, `PostAccept` must be called again immediately to keep at least one pending AcceptEx in flight.
- The AcceptEx function pointer is loaded once in `Initialize()` via `WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)`; cache and reuse it.
- `SO_UPDATE_ACCEPT_CONTEXT` must be set on the accepted socket before using it with other Winsock calls.

### Common Patterns
- `OnCompletion` checks `OverlappedExt.IoType == EIoType::Accept`, extracts the new client socket from `AcceptContext`, then notifies `ServerLifecycle`.
- Error handling on accept failure (e.g., `ERROR_OPERATION_ABORTED` during shutdown) should be silent — log and return without re-posting.

## Dependencies
### Internal
- `network/inc/acceptor/IocpAcceptor.h` — header being implemented
- `network/inc/client/windows/` — OverlappedExt, EIoType

### External
- `mswsock.lib` — AcceptEx, GetAcceptExSockaddrs, SO_UPDATE_ACCEPT_CONTEXT

<!-- MANUAL: -->

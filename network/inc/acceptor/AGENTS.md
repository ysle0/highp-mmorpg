<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/acceptor

## Purpose
AcceptEx-based async connection acceptance. Loads the AcceptEx function pointer via WSAIoctl at runtime and manages the full accept lifecycle for IOCP-based servers.

## Key Files
| File | Description |
|------|-------------|
| IocpAcceptor.h | AcceptEx wrapper; loads AcceptEx via WSAIoctl, posts overlapped accepts, manages accept socket lifecycle |
| AcceptContext.h | Context data passed to accept callbacks — holds client socket and local/remote address buffers |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- `IocpAcceptor` must post new AcceptEx calls after each accepted connection to keep the accept pipeline full.
- AcceptEx requires the accept socket to be created before posting (pre-allocated in `AcceptContext`).
- Address extraction after accept uses `GetAcceptExSockaddrs` — do not parse the buffer manually.

### Common Patterns
- AcceptEx is loaded dynamically: `WSAIoctl(listenSock, SIO_GET_EXTENSION_FUNCTION_POINTER, &WSAID_ACCEPTEX, ...)`.
- `AcceptContext` owns the pre-created client socket; on accept completion it is promoted to an active `Client`.
- Completion is signaled through the IOCP worker via the overlapped pointer in `AcceptContext`.

## Dependencies
### Internal
- `network/inc/io/` — ICompletionTarget interface
- `network/inc/client/windows/` — OverlappedExt, Client

### External
- `mswsock.lib` — AcceptEx, GetAcceptExSockaddrs

<!-- MANUAL: -->

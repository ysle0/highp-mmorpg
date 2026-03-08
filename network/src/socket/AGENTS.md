<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/src/socket

## Purpose
Socket implementations: async socket base, socket helpers, socket option builder, and Windows async socket concrete class.

## Key Files
| File | Description |
|------|-------------|
| AsyncSocket.cpp | Async socket base implementation — non-blocking setup and shared overlapped socket behavior |
| SocketHelper.cpp | Socket utility functions — CreateTcpSocket, SetNonBlocking, address resolution helpers |
| SocketOptionBuilder.cpp | Builder pattern implementation — accumulates socket options and applies them in a single Apply() call |
| WindowsAsyncSocket.cpp | Windows async socket concrete implementation — WSA_FLAG_OVERLAPPED socket creation, IOCP-compatible configuration |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- `WindowsAsyncSocket` must always create sockets with `WSA_FLAG_OVERLAPPED`; omitting this flag silently breaks IOCP.
- `SocketOptionBuilder::Apply` iterates accumulated options and calls `setsockopt` for each; check and log each return value individually.
- `SocketHelper::CreateTcpSocket` is the single creation point — route all socket creation through it rather than calling `WSASocket` directly elsewhere.
- `SetNonBlocking` uses `ioctlsocket(sock, FIONBIO, &mode)` on Windows; do not use `fcntl`.

### Common Patterns
- `IocpAcceptor` calls `SocketHelper::CreateTcpSocket` for both the listen socket and pre-allocated accept sockets.
- `SocketOptionBuilder` usage: `SocketOptionBuilder(sock).ReuseAddr().NoDelay().Apply()`.
- `AsyncSocket` destructor calls `closesocket` if the handle is still valid — ensure ownership is clear before destruction.

## Dependencies
### Internal
- `network/inc/socket/` — headers being implemented
- `network/inc/transport/NetworkTransport.hpp` — ETransport consumed by SocketHelper

### External
- Windows SDK: `winsock2.h`, `ws2tcpip.h`, `ioctlsocket`, `setsockopt`, `ws2_32.lib`

<!-- MANUAL: -->

<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/socket

## Purpose
Socket abstractions and helpers. Provides a layered interface hierarchy from `ISocket` through `AsyncSocket` to `WindowsAsyncSocket`, plus utility and builder helpers for socket configuration.

## Key Files
| File | Description |
|------|-------------|
| ISocket.h | Base socket interface — common socket operations (bind, listen, close) |
| AsyncSocket.h | Async socket base — extends ISocket with non-blocking/overlapped operation contract |
| WindowsAsyncSocket.h | Windows async socket implementation — concrete SOCKET handle management with IOCP-compatible setup |
| SocketHelper.h | Socket utility functions — address resolution, socket creation helpers |
| SocketOptionBuilder.h | Builder pattern for socket options — fluent API for SO_REUSEADDR, TCP_NODELAY, linger, etc. |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- `SocketOptionBuilder` uses method chaining; call `Apply(socket)` at the end to commit all options.
- `WindowsAsyncSocket` must be created with `WSA_FLAG_OVERLAPPED` for IOCP compatibility — `SocketHelper` enforces this.
- Do not call `closesocket` directly; use the `ISocket::Close` interface so RAII and logging are applied consistently.

### Common Patterns
- `IocpAcceptor` and `ServerLifecycle` consume `AsyncSocket` via the abstract interface, not the concrete Windows type.
- `SocketOptionBuilder` is typically used during server socket setup before `bind`/`listen`.
- `SocketHelper` provides `CreateTcpSocket()` and `SetNonBlocking()` free functions used by acceptor and client init paths.

## Dependencies
### Internal
- `network/inc/transport/` — ETransport enum used to parameterize socket creation
- `network/inc/config/` — port and buffer size constants

### External
- Windows SDK: `winsock2.h`, `ws2tcpip.h`, `ws2_32.lib`

<!-- MANUAL: -->

<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/src/client

## Purpose
Client-related implementations: frame assembly buffer, packet stream, TCP client socket, and WSA session management.

## Key Files
| File | Description |
|------|-------------|
| FrameBuffer.cpp | Frame assembly buffer implementation — accumulates recv bytes, detects complete frames by length-prefix |
| PacketStream.cpp | Packet stream implementation — iterates complete frames from FrameBuffer without extra copies |
| TcpClientSocket.cpp | TCP client socket wrapper implementation — SOCKET handle ownership and lifecycle |
| WsaSession.cpp | WSA session RAII implementation — WSAStartup on construct, WSACleanup on destruct |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| windows/ | Windows IOCP Client implementation (Client.cpp) |

## For AI Agents
### Working In This Directory
- `FrameBuffer` must handle partial frames across multiple recv completions — do not assume one recv = one packet.
- `WsaSession` must be constructed exactly once per process before any socket operations; `ServerLifecycle` owns it.
- `PacketStream` is a view over `FrameBuffer` data — it must not allocate; prefer pointer + length pairs.

### Common Patterns
- `FrameBuffer::Consume(n)` advances the read cursor after a packet is processed by `PacketDispatcher`.
- `FrameBuffer::Write(buf, len)` is called from `Client::OnCompletion` after a recv completion.
- `TcpClientSocket` wraps a raw SOCKET but does not manage overlapped I/O — that stays in `Client` (windows/).

## Dependencies
### Internal
- `network/inc/client/` — headers being implemented
- `network/inc/config/Const.h` — buffer size constants

### External
- Windows SDK: `winsock2.h`, `ws2_32.lib`

<!-- MANUAL: -->

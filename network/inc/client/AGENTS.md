<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/client

## Purpose
Per-connection client state and I/O buffer management. Provides TCP socket wrappers, WSA session management, frame assembly buffers, and packet stream abstractions used by the IOCP completion handlers.

## Key Files
| File | Description |
|------|-------------|
| TcpClientSocket.h | TCP client socket wrapper — owns the SOCKET handle for a connected peer |
| WsaSession.h | WSA session management — tracks WSA initialization lifetime |
| FrameBuffer.h | Frame assembly buffer for packet framing — accumulates recv bytes until a complete frame is available |
| PacketStream.h | Packet stream abstraction — reads framed packets from FrameBuffer |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| windows/ | Windows IOCP-specific client implementation (Client, EIoType, OverlappedExt) |

## For AI Agents
### Working In This Directory
- `FrameBuffer` is the boundary between raw IOCP recv completions and structured packet processing.
- Packet framing logic (length-prefix or delimiter) lives in `FrameBuffer`/`PacketStream`; do not duplicate it in `Client`.
- `TcpClientSocket` and `WsaSession` are portable abstractions; platform specifics go in `windows/`.

### Common Patterns
- `FrameBuffer` accumulates bytes from multiple `PostRecv` completions before yielding a full packet.
- `PacketStream` iterates complete frames from a `FrameBuffer` without copying.
- WSA lifetime is managed via RAII in `WsaSession`.

## Dependencies
### Internal
- `network/inc/client/windows/` — IOCP-specific Client, OverlappedExt
- `network/inc/config/` — Const.h for buffer size constants

### External
- Windows SDK: `winsock2.h`, `ws2_32.lib`

<!-- MANUAL: -->

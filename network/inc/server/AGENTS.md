<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/server

## Purpose
Server-side abstractions and packet handling infrastructure. Orchestrates IOCP, acceptor, and client pool; provides the application layer with session event callbacks and a typed packet handler registration system.

## Key Files
| File | Description |
|------|-------------|
| ServerLifecycle.h | Orchestrates IOCP + Acceptor + client pool; handles IOCP completion events; routes recv data to PacketDispatcher and send completions back to Client |
| ISessionEventReceiver.h | Interface for app-layer event callbacks: OnAccept / OnRecv / OnSend / OnDisconnect |
| PacketDispatcher.hpp | Worker thread receives raw bytes -> FrameBuffer assembly -> FlatBuffers parse -> MPSC queue -> logic thread Tick() dispatches to registered handlers |
| IPacketHandler.hpp | Typed payload handler interface; implementations are registered per FlatBuffers payload type tag |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- `ServerLifecycle` is the central wiring point — it holds references to IOCP, acceptor, and the client map.
- `PacketDispatcher` decouples network I/O threads from game logic: I/O threads enqueue parsed commands, the logic thread dequeues in `Tick()`.
- Register packet handlers via `PacketDispatcher` before calling `ServerLifecycle::Start()`; the dispatcher must know all handler types at startup.
- `ISessionEventReceiver` is implemented by the application layer (e.g., `EchoServer`, `ChatServer`) — do not implement it inside `network/`.

### Common Patterns
- MPSC queue in `PacketDispatcher` is lock-free for enqueue (I/O threads) and single-consumer for dequeue (logic thread `Tick()`).
- FlatBuffers payload type tag is the dispatch key for `IPacketHandler` lookup.
- `ServerLifecycle::Stop()` shuts down IOCP, drains the accept queue, then closes all client connections.

## Dependencies
### Internal
- `network/inc/io/` — IocpIoMultiplexer
- `network/inc/acceptor/` — IocpAcceptor
- `network/inc/client/` — Client, FrameBuffer
- `protocol/` — FlatBuffers generated types for packet parsing

### External
- FlatBuffers runtime headers

<!-- MANUAL: -->

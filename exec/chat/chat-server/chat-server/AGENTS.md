<!-- Parent: ../../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# chat-server (project)

## Purpose
Chat server with FlatBuffers-based packet protocol. Uses `PacketDispatcher` for typed message handling with logic thread separation. The double nesting (`chat-server/chat-server/`) is a Visual Studio project structure artifact - the outer directory is the VS solution folder, the inner one is the actual VS project.

## Key Files
| File | Description |
|------|-------------|
| Server.h / Server.cpp | `Server` class - implements `ISessionEventReceiver`, owns `PacketDispatcher` + registered handlers, runs `LogicLoop` on a separate `jthread` with tick-based dispatch |
| main.cpp | Entry point - creates logger, loads runtime config, creates socket, starts server |
| config.runtime.toml | Runtime config: port, max clients, thread counts, tick rate |
| chat-server.vcxproj | Visual Studio project file |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| handlers/ | Per-message-type `IPacketHandler<T>` implementations |

## For AI Agents
### Working In This Directory
- `OnRecv` (called on IOCP worker thread) must only call `PacketDispatcher.Receive()` to enqueue - never do room or game logic on a worker thread.
- `LogicLoop` runs on a dedicated `jthread`; it calls `dispatcher.Tick()` on a fixed interval so all handler logic is single-threaded.
- To add a new message type: add a handler in `handlers/`, register it with `PacketDispatcher` in `Server.cpp`, and regenerate FlatBuffers code from `protocol/`.
- After modifying `config.runtime.toml`, run `scripts/parse_network_cfg.ps1` to regenerate `network/inc/config/NetworkCfg.h`.

### Common Patterns
- Worker thread path: `OnRecv` → `PacketDispatcher.Receive()` → MPSC queue enqueue.
- Logic thread path: `LogicLoop` timer fires → `dispatcher.Tick()` → handler `Handle()` called → room broadcast via `PostSend`.
- Handlers receive a `std::shared_ptr<Client>` and the deserialized FlatBuffers table; they must not block.

## Dependencies
### Internal
- `network/` - `ISessionEventReceiver`, `ServerLifeCycle`, `Client`, `Acceptor`, `IoCompletionPort`
- `lib/` - logger, `Result` type, MPSC queue
- `protocol/` - FlatBuffers-generated message headers

### External
- Windows SDK (Winsock2 / IOCP)
- FlatBuffers runtime headers

<!-- MANUAL: -->

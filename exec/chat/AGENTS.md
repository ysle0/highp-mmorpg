<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# chat

## Purpose
Chat server/client pair - demonstrates FlatBuffers-based packet protocol, typed packet dispatching via `PacketDispatcher`, and room-based messaging. This is the primary reference for adding new message types to the game server.

## Key Files
| File | Description |
|------|-------------|

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| chat-server/ | Chat server with FlatBuffers packet handling and logic thread separation |
| chat-client/ | Chat client for connecting to the chat server |

## For AI Agents
### Working In This Directory
- All packet schemas live in `protocol/` at the repo root; regenerate FlatBuffers code there before touching handler logic here.
- The double-nesting `chat-server/chat-server/` is a Visual Studio project structure artifact - the outer directory is the VS solution folder, the inner one is the actual project.
- Room state is managed server-side; the client only sends requests and receives broadcasts.

### Common Patterns
- New message types require: (1) FlatBuffers schema entry in `protocol/`, (2) a new handler in `chat-server/chat-server/handlers/`, (3) registration in `Server.cpp`'s `PacketDispatcher` setup.
- `OnRecv` feeds raw bytes into `PacketDispatcher.Receive()`; handler logic runs on the logic thread via `dispatcher.Tick()`.

## Dependencies
### Internal
- `network/` - IOCP async I/O, `ISessionEventReceiver`, `ServerLifeCycle`
- `lib/` - logger, `Result` type, `ObjectPool`
- `protocol/` - FlatBuffers-generated message types

### External
- Windows SDK (Winsock2 / IOCP)
- FlatBuffers (schema compiler + runtime headers)

<!-- MANUAL: -->

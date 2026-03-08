<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# handlers

## Purpose
FlatBuffers message handler implementations. Each handler implements `IPacketHandler<T>` for a specific payload type and is called exclusively on the logic thread via `PacketDispatcher::Tick()`.

## Key Files
| File | Description |
|------|-------------|
| ChatMessageHandler.h / ChatMessageHandler.cpp | Handles `SendMessageRequest` - broadcasts `ChatMessageBroadcast` to all clients in the sender's room |
| JoinRoomHandler.h / JoinRoomHandler.cpp | Handles `JoinRoomRequest` - adds client to room, sends `JoinedRoomResponse` to requester and `UserJoinedBroadcast` to existing room members |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- All handlers run on the single logic thread; they must never block or perform I/O directly.
- To add a new handler: (1) create `FooHandler.h/.cpp` implementing `IPacketHandler<FooRequest>`, (2) register it in `Server.cpp`.
- Broadcast helpers (e.g., sending to all room members) belong in the handler or a thin room utility - do not add them to `network/`.
- The FlatBuffers table pointer passed to `Handle()` is valid only for the duration of the call; do not store it.

### Common Patterns
- Handler signature: `void Handle(std::shared_ptr<Client> sender, const FooRequest* msg)`.
- Room membership is tracked via a container owned by `Server` (or a `RoomManager`); handlers receive a reference/pointer to it.
- Outgoing messages are serialized with `flatbuffers::FlatBufferBuilder` inside the handler, then passed to `Client::PostSend()`.

## Dependencies
### Internal
- `network/` - `Client::PostSend()`, session identifiers
- `lib/` - logger
- `protocol/` - FlatBuffers-generated request/response/broadcast types
- `../Server.h` - room state access

### External
- FlatBuffers runtime headers

<!-- MANUAL: -->

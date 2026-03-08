<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# chat-client

## Purpose
Chat client for connecting to the chat server. Sends FlatBuffers-encoded requests and receives broadcast messages, used for manual integration testing of the chat protocol.

## Key Files
| File | Description |
|------|-------------|
| Client.h / Client.cpp | Client implementation - connects, serializes requests, deserializes responses/broadcasts |
| main.cpp | Entry point - connects to the chat server and drives user interaction |
| chat-client.vcxproj | Visual Studio project file |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| room/ | Empty - planned room management logic |

## For AI Agents
### Working In This Directory
- This is a synchronous blocking client; keep it simple relative to the IOCP server.
- All messages must be serialized with `flatbuffers::FlatBufferBuilder` before sending and deserialized with the generated verifier+accessor after receiving.
- The `room/` subdirectory is intentionally empty; add client-side room state tracking there when needed.

### Common Patterns
- Connect → send `JoinRoomRequest` → loop: read user input → send `SendMessageRequest` → print received `ChatMessageBroadcast`.
- Use standard Winsock2 (`connect` / `send` / `recv`) rather than IOCP for simplicity.
- Always verify incoming FlatBuffers buffers with `Verify*` before accessing fields.

## Dependencies
### Internal
- `lib/` - logger
- `protocol/` - FlatBuffers-generated request/response/broadcast types

### External
- Windows SDK (Winsock2)
- FlatBuffers runtime headers

<!-- MANUAL: -->

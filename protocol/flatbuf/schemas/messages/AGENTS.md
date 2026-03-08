<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# messages

## Purpose
FlatBuffers message schema files organized by communication direction. Client-to-server requests, server-to-client responses, and server broadcast messages each live in their own file to make the protocol contract easy to navigate.

## Key Files
| File | Description |
|------|-------------|
| client.fbs | Client-to-server messages: `JoinRoomRequest`, `LeaveRoomRequest`, `SendMessageRequest` |
| server.fbs | Server-to-client response messages: `JoinedRoomResponse` |
| broadcast.fbs | Server-initiated broadcast messages: `UserJoinedBroadcast`, `UserLeftBroadcast`, `ChatMessageBroadcast` |

## Subdirectories
None.

## For AI Agents
### Working In This Directory
- New message types belong in the file that matches their direction:
  - Client initiates → `client.fbs`
  - Server replies to a specific client → `server.fbs`
  - Server sends to all room members → `broadcast.fbs`
- After adding a new table here, register it in `../packet.fbs`:
  1. Add the table to the `Payload` union.
  2. Add a matching entry to the `MessageType` enum in `../enum.fbs`.
  3. Run `scripts/gen_flatbuffers.ps1` to regenerate headers.

### Common Patterns
- All files use `namespace highp.protocol;` and `include` shared types from parent schema files (`../common.fbs`, `../room.fbs`, `../user.fbs`).
- Request tables carry only the fields needed by the server; the server derives identity from the session context, not from the payload.
- Broadcast tables include all fields needed by every recipient (no session context on broadcast path).

## Dependencies
### Internal
- `../packet.fbs` - aggregates these types into the `Payload` union
- `../enum.fbs` - `MessageType` discriminants must match entries here
- `../common.fbs`, `../room.fbs`, `../user.fbs` - shared field types

### External
- FlatBuffers `flatc` compiler (for regeneration)

<!-- MANUAL: -->

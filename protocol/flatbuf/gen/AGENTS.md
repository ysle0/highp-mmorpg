<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# gen

## Purpose
Auto-generated C++ headers produced by the FlatBuffers compiler (`flatc`) from the schemas in `../schemas/`. **Do not edit these files manually** - all changes must be made to the source `.fbs` schemas and then regenerated.

## Key Files
| File | Description |
|------|-------------|
| packet_generated.h | `Packet` root table, `Payload` union, and `PayloadTraits` - the top-level wire type |
| enum_generated.h | `MessageType` enum with `EnumValuesMessageType` and string conversion helpers |
| error_generated.h | `ErrorResponse` table |
| common_generated.h | Shared types used across multiple message definitions |
| room_generated.h | Room-related type definitions |
| user_generated.h | User-related type definitions |
| client_generated.h | Client-to-server message tables (`JoinRoomRequest`, `LeaveRoomRequest`, `SendMessageRequest`) |
| server_generated.h | Server-to-client response tables (`JoinedRoomResponse`) |
| broadcast_generated.h | Server broadcast message tables (`UserJoinedBroadcast`, `UserLeftBroadcast`, `ChatMessageBroadcast`) |

## Subdirectories
None.

## For AI Agents
### Working In This Directory
- **Never edit files in this directory.** Changes will be overwritten on the next code generation run.
- To regenerate after schema changes:
  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts/gen_flatbuffers.ps1
  ```
- Generated headers should be committed to source control so the project builds without requiring `flatc` to be installed.
- If a generated header is missing or out of date, the build will fail with include errors in files that consume the protocol library.

### Common Patterns
- Each generated header is self-contained and includes its FlatBuffers runtime dependency via `"flatbuffers/flatbuffers.h"`.
- Builders follow the pattern `Create<TableName>(builder, field1, field2, ...)`.
- Accessors are `const` methods on the table type, returning scalar values or `flatbuffers::String*` / nested table pointers.
- Union access: call `payload_type()` to get the `Payload` discriminant, then `payload_as_<TypeName>()` to obtain a typed pointer.

## Dependencies
### Internal
- `../schemas/` - source schemas that produce these headers
- `scripts/gen_flatbuffers.ps1` / `gen_flatbuffers.sh` - invokes `flatc`

### External
- FlatBuffers runtime headers (included transitively)

<!-- MANUAL: -->

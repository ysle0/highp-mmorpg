<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# schemas

## Purpose
FlatBuffers schema source files (`.fbs`) defining the chat protocol message format. These are the authoritative definitions for the wire format. Editing these files requires regenerating the headers in `../gen/` via `scripts/gen_flatbuffers.ps1`.

## Key Files
| File | Description |
|------|-------------|
| packet.fbs | Root `Packet` table with `Payload` union aggregating all message types. `file_identifier` is `"CHAT"`. Contains `MessageType`, `Payload`, and `sequence` fields. Declares `root_type Packet`. |
| enum.fbs | `MessageType` enum listing all protocol message discriminants |
| error.fbs | `ErrorResponse` table for server-to-client error messages |
| common.fbs | Shared types referenced by multiple other schemas |
| room.fbs | Room-related type definitions |
| user.fbs | User-related type definitions |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| messages/ | Request, response, and broadcast message schemas organized by direction |

## For AI Agents
### Working In This Directory
- Adding a new message type requires changes in multiple files:
  1. Define the new table in the appropriate file under `messages/` (or here if it is a shared type).
  2. Add a new variant to the `Payload` union in `packet.fbs`.
  3. Add a discriminant to the `MessageType` enum in `enum.fbs`.
  4. Regenerate headers: `powershell -ExecutionPolicy Bypass -File scripts/gen_flatbuffers.ps1`.
- The `file_identifier "CHAT"` in `packet.fbs` is a 4-byte magic that FlatBuffers embeds in the buffer. Do not change it without coordinating a protocol version bump.

### Common Patterns
- All schemas use `namespace highp.protocol;`.
- Tables use snake_case field names; FlatBuffers generates camelCase C++ accessors.
- The `Payload` union is the sole extension point for new top-level message types.
- `sequence` field on `Packet` carries a monotonically increasing client-side counter for duplicate detection / ordering.

## Dependencies
### Internal
- `../gen/` - output of compilation; consumed by `network/` and application layers

### External
- FlatBuffers `flatc` compiler

<!-- MANUAL: -->

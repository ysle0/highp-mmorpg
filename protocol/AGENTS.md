<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# protocol

## Purpose
FlatBuffers-based protocol library. Contains schemas defining the wire format and generated C++ serialization headers. Provides the shared message contract between server and client under the namespace `highp::protocol`.

## Key Files
| File | Description |
|------|-------------|
| protocol.cpp | Library entry point / precompiled header driver |
| pch.h | Precompiled header - common includes |
| pch.cpp | Precompiled header compilation unit |
| framework.h | Windows/platform framework includes |
| protocol.vcxproj | Visual Studio project file |
| flatbuffers.natvis | Visual Studio debugger visualization for FlatBuffers types |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| flatbuf/ | FlatBuffers schema definitions and auto-generated C++ headers |

## For AI Agents
### Working In This Directory
- Do not manually edit files inside `flatbuf/gen/` - they are auto-generated.
- After modifying any `.fbs` schema under `flatbuf/schemas/`, run the generation script:
  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts/gen_flatbuffers.ps1
  ```
- The Visual Studio project (`protocol.vcxproj`) references both the schema files and the generated headers. Keep the filters file (`protocol.vcxproj.filters`) in sync if adding new schemas.

### Common Patterns
- All protocol types live under the `highp::protocol` namespace (defined in generated headers).
- The root wire type is `Packet` (see `flatbuf/gen/packet_generated.h`). All messages are wrapped in a `Packet` before being sent over the wire.
- Use `flatbuffers::FlatBufferBuilder` to construct messages and `flatbuffers::GetRoot<highp::protocol::Packet>()` to deserialize.

## Dependencies
### Internal
- `lib/` - shared utilities (logging, error handling)
- `network/` consumes this library to decode incoming buffers and encode outgoing messages

### External
- FlatBuffers (Google) - schema compiler (`flatc`) and runtime headers

<!-- MANUAL: -->

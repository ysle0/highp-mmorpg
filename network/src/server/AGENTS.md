<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/src/server

## Purpose
Server lifecycle implementation and PacketDispatcher. Contains the Receive/Tick/ParsePacket/DispatchPacket pipeline that bridges raw IOCP completions to typed application-layer handlers.

## Key Files
| File | Description |
|------|-------------|
| PacketDispatcher.cpp | Receive() feeds bytes into FrameBuffer; ParsePacket() runs FlatBuffers verification + parse; MPSC enqueue; Tick() dequeues and calls registered IPacketHandler per payload type |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- `Receive()` is called from I/O worker threads — it must be thread-safe up to the MPSC enqueue point.
- `Tick()` is called from the single logic thread — it is the only consumer of the MPSC queue; no locking needed on the dequeue side.
- FlatBuffers verification (`flatbuffers::Verifier`) runs inside `ParsePacket()` before any field access — never skip verification on untrusted input.
- If `ParsePacket()` fails verification, drop the packet and close the client connection (malformed packet = protocol violation).

### Common Patterns
- Pipeline: `Receive(buf, len)` -> `FrameBuffer::Write` -> complete frame detected -> `ParsePacket()` -> enqueue command -> `Tick()` -> lookup handler by payload type -> `IPacketHandler::Handle(payload, client)`.
- Handler map is populated at startup before the server starts accepting; it is read-only during `Tick()` — no synchronization needed.
- `PacketDispatcher` owns the MPSC queue; `ServerLifecycle` owns `PacketDispatcher`.

## Dependencies
### Internal
- `network/inc/server/PacketDispatcher.hpp` — header being implemented
- `network/inc/server/IPacketHandler.hpp` — handler interface invoked in Tick()
- `network/inc/client/FrameBuffer.h` — frame assembly
- `protocol/` — FlatBuffers generated verifier and accessor types

### External
- FlatBuffers runtime: `flatbuffers/flatbuffers.h`, `flatbuffers::Verifier`

<!-- MANUAL: -->

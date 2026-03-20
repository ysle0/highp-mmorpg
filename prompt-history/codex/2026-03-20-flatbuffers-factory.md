# 2026-03-20

- Topic: Move `Room::Prepare*` helpers into `highp::protocol` and consider `flatc` automation.
- Decision: Keep `protocol/flatbuf/gen/*` read-only and add a hand-written `highp::protocol` factory layer on top.
- Shape: Centralize `CreatePacket(...)/FinishPacketBuffer(...)` and reuse generated `PayloadTraits<T>` for packet envelope creation.
- Boundary: Let callers pass timestamps instead of making `protocol` depend on `highp::time`.
- Automation: Stock `flatc` does not generate custom packet factories from this schema; if needed, add a post-step after `scripts/gen_flatbuffers.ps1` or generate from `.bfbs` IR.
- Bug noted: `UserLeftBroadcast` schema requires `username`, but current `Room::PrepareUserLeft` only passes `user_id`.

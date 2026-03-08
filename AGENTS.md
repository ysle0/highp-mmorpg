<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# highp-mmorpg

## Purpose
Windows IOCP-based high-performance async MMORPG server backend. Currently implements echo and chat servers as foundations for game server development. Uses C++17/20 with Visual Studio 2022.

## Key Files

| File | Description |
|------|-------------|
| `highp-mmorpg.slnx` | Visual Studio solution file |
| `CLAUDE.md` | AI agent instructions and project overview |
| `README.md` | Project readme |

## Subdirectories

| Directory | Purpose |
|-----------|---------|
| `network/` | IOCP networking core - async I/O, acceptor, client management (see `network/AGENTS.md`) |
| `lib/` | Shared utilities - Result type, logger, error handling, memory pools (see `lib/AGENTS.md`) |
| `protocol/` | FlatBuffers schemas and generated serialization code (see `protocol/AGENTS.md`) |
| `exec/` | Runnable server/client applications (see `exec/AGENTS.md`) |
| `scripts/` | TOML-to-C++ header generators and FlatBuffers codegen (see `scripts/AGENTS.md`) |
| `docs/` | Architecture and reference documentation (see `docs/AGENTS.md`) |

## For AI Agents

### Architecture Overview
```
Application Layer (exec/)     ← Business logic (EchoServer, ChatServer)
        ↓ implements ISessionEventReceiver
Network Layer (network/)      ← IOCP, sockets, async I/O primitives
        ↓ uses
Utility Layer (lib/)          ← Result<T,E>, Logger, error types, memory pools
        ↓ serializes with
Protocol Layer (protocol/)    ← FlatBuffers schemas and generated code
```

### Async I/O Flow
```
AcceptEx → GQCS wakes worker → OnAccept → Client.PostRecv()
                                             ↓
Client sends → GQCS wakes worker → OnRecv → PacketDispatcher.Receive()
  → FrameBuffer assembles → ParsePacket → PushCommand to MPSC queue
  → Logic thread Tick() → Dispatch to IPacketHandler
```

### Critical Invariants
- `OverlappedExt.overlapped` (WSAOVERLAPPED) MUST be the first struct member for `reinterpret_cast` from OVERLAPPED*
- Use `Result<T, E>` + `GUARD(...)` for error propagation, not exceptions
- `std::shared_ptr<Client>` for connection lifetime management
- PascalCase for types/methods, camelCase for locals/params, `_camelCase` for private members

### Build Commands
```powershell
msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64 /m
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1   # after config.compile.toml changes
powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1   # after config.runtime.toml changes
powershell -ExecutionPolicy Bypass -File scripts/gen_flatbuffers.ps1     # after .fbs changes
```

### Testing
Manual integration testing only. Start server, then client, verify echo round-trip / chat messaging.

### Commit Style
`feat(scope): ...`, `refactor(scope): ...`, `fix(scope): ...`, `docs: ...`, `cfg: ...`, `script: ...`

<!-- MANUAL: Any manually added notes below this line are preserved on regeneration -->

<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network

## Purpose
Static library providing IOCP-based async networking primitives. Core classes: IocpIoMultiplexer (IOCP handle + worker thread pool), IocpAcceptor (AcceptEx-based async accept), Client (per-connection state + I/O buffers), ServerLifeCycle (orchestrates IOCP, acceptor, client pool), PacketDispatcher (frame assembly + FlatBuffers parsing + MPSC command queue dispatch).

## Key Files
| File | Description |
|------|-------------|
| network.cpp | Library entry point |
| pch.h | Precompiled header declarations |
| pch.cpp | Precompiled header compilation unit |
| framework.h | Common framework includes |
| config.compile.toml | Compile-time config source (buffer sizes); regenerate with scripts/parse_compile_cfg.ps1 |
| network.vcxproj | MSVC project file for the static library |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| inc/ | Public headers organized by subsystem |
| src/ | Implementation files |

## For AI Agents
### Working In This Directory
- This is a static library; all changes here affect every executable that links against it (echo-server, chat-server).
- After modifying `config.compile.toml`, run `scripts/parse_compile_cfg.ps1` to regenerate `inc/config/Const.h`.
- Build with MSBuild from a VS 2022 Developer Command Prompt: `msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64`.

### Common Patterns
- Async I/O follows the IOCP completion pattern: post an operation, worker thread calls GQCS, dispatches via `ICompletionTarget`.
- `OverlappedExt.overlapped` MUST remain the first struct member; pointer arithmetic depends on it.
- Use `Result<T, E>` for error propagation rather than exceptions.

## Dependencies
### Internal
- `lib/` — Result type, logging, error handling, memory utilities

### External
- Windows SDK: `ws2_32.lib`, `mswsock.lib` (AcceptEx, WSARecv, WSASend, IOCP APIs)
- FlatBuffers (protocol/ generated code for packet parsing)

<!-- MANUAL: -->

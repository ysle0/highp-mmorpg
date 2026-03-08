<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# echo-server

## Purpose
IOCP-based async echo server. Implements `ISessionEventReceiver` to handle `OnRecv` by echoing received data back via `PostSend`. Serves as the reference implementation for building more complex servers on top of the network library.

## Key Files
| File | Description |
|------|-------------|
| Server.h / Server.cpp | `Server` class - implements `ISessionEventReceiver`, owns `ServerLifeCycle`, delegates all networking to the network library |
| main.cpp | Entry point - creates logger, loads runtime config from `config.runtime.toml`, creates socket, starts server |
| config.runtime.toml | Runtime configuration: port, max clients, worker thread counts |
| echo-server.vcxproj | Visual Studio project file |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- `Server` is the only business-logic class; keep it thin.
- `OnRecv` should immediately call `PostSend` with the same buffer - no queuing needed for an echo server.
- After modifying `config.runtime.toml`, run `scripts/parse_network_cfg.ps1` to regenerate `network/inc/config/NetworkCfg.h`.
- The working directory when running the executable must contain `config.runtime.toml` (the TOML is read at startup from CWD).

### Common Patterns
- `Server` owns a `ServerLifeCycle` member (not a pointer) and calls its methods to start/stop.
- Implement `ISessionEventReceiver` callbacks (`OnAccept`, `OnRecv`, `OnDisconnect`) as the sole business logic hook points.
- Use `Result<T, E>` for error propagation; log and return early on failure.

## Dependencies
### Internal
- `network/` - `ISessionEventReceiver`, `ServerLifeCycle`, `Client`, `Acceptor`, `IoCompletionPort`
- `lib/` - logger, `Result` type

### External
- Windows SDK (Winsock2 / IOCP)

<!-- MANUAL: -->

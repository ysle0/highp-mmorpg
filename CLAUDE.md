# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Windows IOCP-based high-performance async MMORPG server backend. Currently implements an echo server as the foundation for game server development.

## Build Commands

**Build System:** Visual Studio 2022 (MSBuild), C++17/20

```powershell
# Open solution
start highp-mmorpg.slnx

# Build from command line (requires VS Developer Command Prompt)
msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64
msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64

# Regenerate config headers after modifying .toml files
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1
```

## Testing

No automated test framework. Manual integration testing via echo client:

```powershell
# Terminal 1: Start server (reads config.runtime.toml from working directory)
.\x64\Debug\echo-server.exe

# Terminal 2: Run test client (connects to 127.0.0.1:8080)
.\x64\Debug\echo-client.exe
```

## Architecture

### Layer Separation

| Layer | Location | Purpose |
|-------|----------|---------|
| Network | `network/` | IOCP, sockets, async I/O primitives |
| Application | `echo/echo-server/` | Business logic (EchoServer) |
| Utilities | `lib/` | Result type, logging, error handling |

### Core Classes

- **IoCompletionPort**: Manages IOCP handle and worker thread pool
- **Acceptor**: AcceptEx-based async connection acceptance
- **Client**: Per-connection state and I/O buffers (WSARecv/WSASend)
- **EchoServer**: Business logic layer, orchestrates components

### Async I/O Flow

```
AcceptEx → GQCS wakes worker → OnAccept → Client.PostRecv()
                                             ↓
Client sends → GQCS wakes worker → OnRecv → PostSend(echo) → PostRecv()
```

### Critical Invariant

`OverlappedExt.overlapped` (WSAOVERLAPPED) MUST be the first struct member for `reinterpret_cast` from OVERLAPPED* to work correctly.

## Configuration System

**Compile-time** (`network/config.compile.toml` → `network/Const.h`):
- Buffer sizes (recv: 4096, send: 1024)
- Run `parse_compile_cfg.ps1` after changes

**Runtime** (`echo/echo-server/config.runtime.toml` → `network/NetworkCfg.h`):
- Server port, max clients, thread counts
- Supports env var overrides: `SERVER_PORT`, `SERVER_MAX_CLIENTS`
- Run `parse_network_cfg.ps1` after changes

## Code Patterns

- **Error handling**: Use `Result<T, E>` type (similar to C++23 expected)
- **Memory**: `std::shared_ptr<Client>` for connection lifetime, `ObjectPool<OverlappedExt>` for I/O contexts
- **Threading**: `std::jthread` for worker threads with automatic cleanup
- **Naming**: PascalCase for types/methods, camelCase for members

## Project Structure

```
network/          # Static library - async I/O primitives
echo/echo-server/ # Echo server executable
echo/echo-client/ # Test client executable
lib/              # Shared utilities (Result, Logger, Errors)
scripts/          # TOML → C++ header generators
docs/             # Architecture documentation
```

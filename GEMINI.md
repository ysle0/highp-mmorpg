# Gemini Context: High-Performance MMORPG Backend

This file provides context for the Gemini AI agent working on the `highp-mmorpg` project.

## Project Overview

**Type:** C++20 Windows IOCP-based Asynchronous Game Server Backend
**Goal:** Build a scalable, high-performance foundation for MMORPGs, starting with an Echo Server and evolving into a Chat and Game Server.
**Platform:** Windows (win32)
**Build System:** Visual Studio 2022 (MSBuild)

## Directory Structure

* **`network/`**: **Core Library.** Contains the async I/O engine.
  * `IocpIoMultiplexer`: Manages the IOCP handle and worker threads.
  * `IocpAcceptor`: Handles async connection acceptance via `AcceptEx`.
  * `ServerCore` (LifeCycle): Orchestrates the server (IOCP, Acceptor, Client pool).
  * `Client`: Represents a connected session (buffers, state).
* **`exec/`**: **Applications.**
  * `echo/`: Echo server and client implementation.
    * `echo-server/`: Main entry point for the echo server.
  * `chat/`: Chat server implementation (Work In Progress).
* **`lib/`**: **Utilities.**
  * `Logger`: Async text/console logging.
  * `Result`: Functional error handling (`Result<T, E>`).
  * `ObjectPool`: Memory pooling for performance-critical objects (e.g., `OverlappedExt`).
* **`protocol/`**: **Data Serialization.**
  * Uses Google FlatBuffers.
  * `schemas/`: `.fbs` definition files.
* **`docs/`**: Extensive documentation (Architecture, Network Layer, API Reference).
* **`scripts/`**: PowerShell scripts for code generation (Configs, Flatbuffers).

## Build & Run

### Prerequisites

* Visual Studio 2022
* PowerShell

### Configuration (Crucial Step)

The project uses a two-stage configuration system. **You must run these scripts if config files change or before the first build.**

1. **Compile-Time Config** (Buffer sizes, etc.):
    * Source: `network/config.compile.toml`
    * Generator: `scripts/parse_compile_cfg.ps1`
    * Output: `network/Const.h`
    * *Requires Rebuild if changed.*

2. **Runtime Config** (Ports, Threads, etc.):
    * Source: `exec/echo/echo-server/config.runtime.toml`
    * Generator: `scripts/parse_network_cfg.ps1`
    * Output: `network/NetworkCfg.h`
    * *Values can also be overridden by Environment Variables (e.g., `SERVER_PORT`).*

### Build Commands

Run in a Visual Studio Developer Command Prompt or via `run_shell_command` with `msbuild`.

```powershell
# Debug Build
msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64

# Release Build
msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64
```

### Running the Echo Server

```powershell
# Start Server
.\x64\Debug\echo-server.exe

# Start Test Client (in a separate terminal)
.\x64\Debug\echo-client.exe
```

## Architecture & Design Patterns

* **IOCP (I/O Completion Ports):** The heart of the concurrency model.
  * **Proactor Pattern:** Operations (Recv, Send, Accept) are initiated asynchronously. Completion events are dequeued by worker threads.
  * **Threading:** One main thread + `N` worker threads (handling IOCP completions).
* **Layering:**
  * **Application (`EchoServer`):** Implements `IServerHandler`. Focuses on business logic.
  * **Network (`ServerLifeCycle`):** Manages the plumbing. Calls `IServerHandler` methods.
* **Memory Management:**
  * `std::shared_ptr<Client>`: Manages connection lifetime.
  * `ObjectPool`: Used for `OverlappedExt` to reduce allocation overhead.
  * **Invariant:** `OverlappedExt` **MUST** have `WSAOVERLAPPED` as its *first member* to allow safe `reinterpret_cast`.
* **Error Handling:**
  * Uses `Result<T, E>` (monadic error handling) instead of exceptions for control flow.

## Coding Conventions

* **Style:**
  * **Types/Methods:** PascalCase (e.g., `ServerLifeCycle`, `Start`).
  * **Variables/Members:** camelCase (e.g., `recvBuffer`, `_clientPool`).
  * **Member Prefix:** `_` for private members (e.g., `_handler`).
* **Modern C++:** Uses C++20 features (concepts, `std::span`, `std::jthread`).
* **No Exceptions:** Prefer `Result<T, E>` for predictable error paths.

## Key Files for AI Context

* `network/ServerCore.h`: The main interface between the network engine and the application.
* `exec/echo/echo-server/EchoServer.cpp`: Example usage of the network engine.
* `docs/ARCHITECTURE.md`: Detailed architectural decisions.
* `network/Client.h`: Understanding per-connection state.

## Common Tasks

* **Adding a new feature:** Implement logic in `EchoServer` (or a new app) via `IServerHandler`.
* **Changing Buffer Sizes:** Edit `network/config.compile.toml` -> Run `scripts/parse_compile_cfg.ps1` -> Rebuild.
* **Debugging:** Check `logs/` (if configured) or standard output. The `Logger` class handles this.

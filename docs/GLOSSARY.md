# Glossary

Quick reference for technical terms and acronyms used throughout the project.

---

## A

**AcceptEx**
- Microsoft Winsock extension for asynchronous connection acceptance
- Requires function pointer loaded via `WSAIoctl()`
- See: [Network Layer Guide](NETWORK_LAYER.md#iocpacceptor)

**Async I/O**
- Non-blocking I/O operations that return immediately
- Completion notifications delivered via IOCP
- See: [IOCP Concepts](NETWORK_LAYER.md#iocp-and-async-io-concepts)

---

## C

**Completion Key**
- User-defined pointer passed during IOCP socket association
- In this project: Always `Client*` pointer
- Allows immediate connection identification in completion events
- See: [IOCP Concepts](NETWORK_LAYER.md#key-iocp-concepts)

**Compile-time Configuration**
- Constants baked into binary at compile time (`Const.h`)
- Used for buffer sizes and memory layout
- Requires rebuild to change
- See: [Configuration Guide](CONFIGURATION.md#compile-time-configuration)

---

## E

**Echo Server**
- Reference implementation demonstrating network layer
- Echoes received data back to sender
- Located in `exec/echo/echo-server/`
- See: [Build and Test Guide](BUILD_AND_TEST.md#echo-server)

**Error Handling**
- Uses `Result<T, E>` monad (similar to C++23 `std::expected`)
- No exceptions in hot paths
- Explicit error propagation
- See: [Error Handling Guide](ERROR_HANDLING.md)

---

## F

**FlatBuffers**
- Zero-copy binary serialization library by Google
- Used for client-server protocol messages
- Schema-driven type-safe serialization
- See: [Protocol Documentation](PROTOCOL.md#flatbuffers-integration)

---

## G

**GQCS**
- `GetQueuedCompletionStatus()` - Windows API function
- Blocks worker thread until I/O completion or timeout
- Core of IOCP event loop
- See: [IOCP Workflow](NETWORK_LAYER.md#iocp-workflow)

---

## I

**IOCP (I/O Completion Port)**
- Windows kernel mechanism for scalable async I/O multiplexing
- Manages thread wake-ups and completion notifications
- Handles thousands of concurrent connections efficiently
- See: [IOCP Concepts](NETWORK_LAYER.md#iocp-and-async-io-concepts)

**IocpAcceptor**
- Component handling `AcceptEx` async connection acceptance
- Manages accept socket pool and overlapped structures
- Automatically re-posts accepts to maintain backlog
- See: [API Reference](API_REFERENCE.md#iocpacceptor)

**IocpIoMultiplexer**
- Core IOCP wrapper managing worker thread pool
- Dispatches completion events to registered handlers
- See: [API Reference](API_REFERENCE.md#iocpiomultiplexer)

**IServerHandler**
- Interface for application-layer event callbacks
- Methods: `OnAccept`, `OnRecv`, `OnSend`, `OnDisconnect`
- Implemented by business logic layer (e.g., `EchoServer`)
- See: [API Reference](API_REFERENCE.md#iserverhandler)

---

## L

**Layer Separation**
- Architecture principle: Network ↔ Application ↔ Utilities
- Network layer isolated from business logic
- See: [Architecture Overview](ARCHITECTURE.md#layer-separation)

---

## M

**MSBuild**
- Microsoft Build Engine (Visual Studio's build system)
- Used to compile C++ projects from `.vcxproj` files
- See: [Build Commands](BUILD_AND_TEST.md#build-commands)

---

## O

**ObjectPool**
- Memory pool for frequently allocated/deallocated objects
- Used for `AcceptOverlapped` structures
- Reduces allocation overhead in hot paths
- See: [API Reference](API_REFERENCE.md#objectpoolt)

**Overlapped I/O**
- Windows async I/O model using `WSAOVERLAPPED` structure
- Extended to `OverlappedExt` with I/O type and buffers
- See: [Network Layer Guide](NETWORK_LAYER.md#overlappedext-structures)

**OverlappedExt**
- Extended OVERLAPPED structure with metadata
- Types: `SendOverlapped`, `RecvOverlapped`, `AcceptOverlapped`
- **Critical:** `WSAOVERLAPPED overlapped` MUST be first member
- See: [API Reference](API_REFERENCE.md#overlappedext)

---

## R

**Result<T, E>**
- Error handling monad type (similar to C++23 `std::expected`)
- Returns either success value (`T`) or error (`E`)
- Enables explicit error handling without exceptions
- See: [API Reference](API_REFERENCE.md#resultt-e)

**Runtime Configuration**
- Settings loaded at startup from `config.runtime.toml`
- Can be overridden via environment variables
- No rebuild required to change
- See: [Configuration Guide](CONFIGURATION.md#runtime-configuration)

---

## S

**ServerLifeCycle**
- High-level server lifecycle orchestrator
- Manages IOCP, Acceptor, and client pool
- Bridges network events to `IServerHandler` callbacks
- See: [API Reference](API_REFERENCE.md#serverlifecycle)

---

## T

**TOML**
- Configuration file format (Tom's Obvious Minimal Language)
- Used for both compile-time and runtime configs
- Parsed into C++ headers via PowerShell scripts
- See: [Configuration Guide](CONFIGURATION.md#toml-parsing-and-code-generation)

---

## W

**Worker Thread**
- Thread blocked on `GetQueuedCompletionStatus()` waiting for I/O completion
- Count = `hardware_concurrency() × max_worker_thread_multiplier`
- Typical: 2× CPU cores for I/O-bound workloads
- See: [Threading Model](ARCHITECTURE.md#threading-model)

**WSARecv / WSASend**
- Windows async socket receive/send API functions
- Return immediately with `ERROR_IO_PENDING`
- Completion notification via IOCP
- See: [Async I/O Patterns](NETWORK_LAYER.md#async-io-patterns)

---

## Z

**Zero-Copy**
- Accessing data directly without intermediate copying
- FlatBuffers enables zero-copy deserialization
- Performance benefit for high-throughput servers
- See: [Protocol Performance](PROTOCOL.md#zero-copy-deserialization)

---

## Acronyms

| Acronym | Full Name | Description |
|---------|-----------|-------------|
| **API** | Application Programming Interface | Programming interface for system interaction |
| **GQCS** | GetQueuedCompletionStatus | Windows API function for IOCP event retrieval |
| **IOCP** | I/O Completion Port | Windows async I/O multiplexing mechanism |
| **MMORPG** | Massively Multiplayer Online Role-Playing Game | Target application domain |
| **MSVC** | Microsoft Visual C++ | C++ compiler toolset |
| **RAII** | Resource Acquisition Is Initialization | C++ resource management idiom |
| **RPC** | Remote Procedure Call | Request/response communication pattern |
| **TCP** | Transmission Control Protocol | Connection-oriented network protocol |
| **TOML** | Tom's Obvious Minimal Language | Configuration file format |

---

## See Also

- **[INDEX](INDEX.md)** - Documentation index and navigation
- **[QUICK_START](QUICK_START.md)** - Get running in 5 minutes
- **[API_REFERENCE](API_REFERENCE.md)** - Complete API documentation
- **[ARCHITECTURE](ARCHITECTURE.md)** - System architecture and design

---

**Last Updated:** 2026-02-05


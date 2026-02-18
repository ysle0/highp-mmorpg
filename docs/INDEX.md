# highp-mmorpg Documentation Index

> **Project:** Windows IOCP-based High-Performance Async MMORPG Server Backend
> **Status:** Foundation (Echo Server Implementation Complete)
> **Build System:** Visual Studio 2022 (MSBuild), C++17/20

---

## 🚀 New Here? Start Here!

**First Time?** → [Quick Start Guide](QUICK_START.md) - Get running in 5 minutes
**Building?** → [Build and Test](BUILD_AND_TEST.md#quick-reference) - Build commands
**Learning?** → [Learning Path](#-learning-path) below - Structured guide

---

## 📚 Documentation Structure

### Core Documentation
- **[Quick Start Guide](QUICK_START.md)** - Get running in under 5 minutes ⚡
- **[Architecture Overview](ARCHITECTURE.md)** - System architecture, design patterns, and layer separation
- **[API Reference](API_REFERENCE.md)** - Complete API documentation for all classes and functions
- **[Network Layer Guide](NETWORK_LAYER.md)** - IOCP, async I/O, and network primitives
- **[Protocol Specification](PROTOCOL.md)** - FlatBuffers schemas and message formats
- **[Configuration Guide](CONFIGURATION.md)** - Compile-time and runtime configuration

### Implementation Guides
- **[IOCP Echo Server Architecture](IOCP_EchoServer_Architecture.md)** - Detailed echo server implementation
- **[Echo Server Sequence Diagrams](EchoServer_Sequence_diagram.md)** - Visual flow diagrams
- **[Building and Testing](BUILD_AND_TEST.md)** - Build instructions, testing procedures

### Reference Materials
- **[Error Handling](ERROR_HANDLING.md)** - Result types, error codes, and error handling patterns
- **[Utilities Reference](UTILITIES.md)** - Logger, ObjectPool, Result<T, E>, and helpers
- **[Code Style Guide](CODE_STYLE.md)** - Naming conventions and coding standards
- **[Glossary](GLOSSARY.md)** - Technical terms and acronyms quick reference 📖

---

## 🏗️ Quick Navigation

### By Component

| Component | Description | Documentation |
|-----------|-------------|---------------|
| **IocpIoMultiplexer** | IOCP handle management and worker thread pool | [Network Layer Guide](NETWORK_LAYER.md#iocpiomultiplexer) |
| **IocpAcceptor** | AcceptEx-based async connection acceptance | [Network Layer Guide](NETWORK_LAYER.md#iocpacceptor) |
| **Client** | Per-connection state and I/O buffers | [Network Layer Guide](NETWORK_LAYER.md#client) |
| **ServerLifeCycle** | Server lifecycle and common logic | [Network Layer Guide](NETWORK_LAYER.md#serverlifecycle) |
| **EchoServer** | Business logic layer (Echo implementation) | [Architecture Overview](ARCHITECTURE.md#application-layer) |

### By Topic

| Topic | Documentation |
|-------|---------------|
| **Getting Started** | [README.md](../README.md), [CLAUDE.md](../CLAUDE.md) |
| **IOCP Concepts** | [IOCP Echo Server Architecture](IOCP_EchoServer_Architecture.md) |
| **Async I/O Flow** | [Echo Server Sequence Diagrams](EchoServer_Sequence_diagram.md) |
| **Protocol Design** | [Protocol Specification](PROTOCOL.md) |
| **Error Handling** | [Error Handling Guide](ERROR_HANDLING.md) |
| **Configuration** | [Configuration Guide](CONFIGURATION.md) |

---

## 🚀 Quick Start

1. **Build the Project**
   ```powershell
   # Open solution
   start highp-mmorpg.slnx

   # Or build from command line
   msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64
   ```

2. **Run Echo Server**
   ```powershell
   .\x64\Release\echo-server.exe
   ```

3. **Test with Client**
   ```powershell
   .\x64\Release\echo-client.exe
   ```

**See:** [Building and Testing Guide](BUILD_AND_TEST.md) for detailed instructions.

---

## 📐 Architecture Summary

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  (EchoServer, ChatServer - Business Logic)                  │
├─────────────────────────────────────────────────────────────┤
│                    Network Layer                            │
│  (ServerLifeCycle, IocpIoMultiplexer, IocpAcceptor, Client) │
├─────────────────────────────────────────────────────────────┤
│                    Utilities Layer                          │
│  (Logger, Result<T,E>, ObjectPool, Error Handling)          │
└─────────────────────────────────────────────────────────────┘
```

**Key Design Principles:**
- **Layer Separation:** Network primitives isolated from business logic
- **Async I/O:** IOCP-based non-blocking I/O with worker thread pool
- **Memory Management:** ObjectPool for OVERLAPPED contexts, shared_ptr for clients
- **Error Handling:** Result<T, E> type (similar to C++23 expected)

**See:** [Architecture Overview](ARCHITECTURE.md) for detailed design information.

---

## 🔑 Key Concepts

### IOCP (I/O Completion Port)
Windows kernel-level I/O multiplexing mechanism. Completion notifications are delivered to worker threads via `GetQueuedCompletionStatus()`.

**Learn More:** [IOCP Echo Server Architecture](IOCP_EchoServer_Architecture.md#iocp)

### Overlapped I/O
Asynchronous I/O operations using WSAOVERLAPPED structure. The `OverlappedExt` structure extends WSAOVERLAPPED with context information (I/O type, buffers).

**Critical Invariant:** `WSAOVERLAPPED overlapped` MUST be the first struct member for safe `reinterpret_cast`.

**Learn More:** [Network Layer Guide](NETWORK_LAYER.md#overlappedext)

### AcceptEx
Microsoft Winsock extension for async connection acceptance. Requires indirect function pointer acquisition via `WSAIoctl`.

**Learn More:** [Network Layer Guide](NETWORK_LAYER.md#iocpacceptor)

---

## 📦 Project Structure

```
highp-mmorpg/
├── network/              # Network layer (IOCP, async I/O primitives)
│   ├── IocpIoMultiplexer.h/cpp
│   ├── IocpAcceptor.h/cpp
│   ├── ServerLifecycle.h/cpp
│   ├── Client.h/cpp
│   ├── OverlappedExt.h
│   ├── NetworkCfg.h      # Auto-generated runtime config
│   └── Const.h           # Auto-generated compile-time constants
│
├── exec/                 # Executables
│   ├── echo/             # Echo server and client
│   │   ├── echo-server/
│   │   └── echo-client/
│   └── chat/             # Chat server (in progress)
│
├── lib/                  # Shared utilities
│   ├── Result.hpp        # Result<T, E> type
│   ├── Logger.hpp        # Logging system
│   ├── NetworkError.h    # Network error codes
│   └── ObjectPool.hpp    # Object pool allocator
│
├── protocol/             # Protocol definitions
│   └── flatbuf/          # FlatBuffers schemas
│       └── schemas/
│
├── scripts/              # Build automation
│   ├── parse_compile_cfg.ps1   # TOML → Const.h
│   └── parse_network_cfg.ps1   # TOML → NetworkCfg.h
│
└── docs/                 # Documentation (you are here)
    ├── INDEX.md
    ├── ARCHITECTURE.md
    ├── API_REFERENCE.md
    └── ...
```

---

## 🔗 Cross-References

### From README.md
- Project overview and quick start → This index
- Build commands → [Building and Testing](BUILD_AND_TEST.md)
- Architecture summary → [Architecture Overview](ARCHITECTURE.md)

### From CLAUDE.md
- Claude Code guidance → Development context
- Project structure → This index
- Core classes → [API Reference](API_REFERENCE.md)

### From Source Code
- Header documentation → [API Reference](API_REFERENCE.md)
- Error codes → [Error Handling Guide](ERROR_HANDLING.md)
- Configuration → [Configuration Guide](CONFIGURATION.md)

---

## 📝 Documentation Maintenance

### Updating Documentation

When modifying the codebase, update relevant documentation:

1. **API Changes** → Update [API Reference](API_REFERENCE.md)
2. **Architecture Changes** → Update [Architecture Overview](ARCHITECTURE.md)
3. **Protocol Changes** → Update [Protocol Specification](PROTOCOL.md)
4. **Configuration Changes** → Update [Configuration Guide](CONFIGURATION.md)

### Auto-Generated Files

The following files are auto-generated and should not be manually edited:
- `network/NetworkCfg.h` (from `config.runtime.toml`)
- `network/Const.h` (from `config.compile.toml`)
- `protocol/flatbuf/gen/*.h` (from `*.fbs` schemas)

**See:** [Configuration Guide](CONFIGURATION.md#auto-generation) for regeneration commands.

---

## 🎯 Learning Path

### For New Contributors

1. **Start Here:** [README.md](../README.md) - Project overview
2. **Understand Architecture:** [Architecture Overview](ARCHITECTURE.md)
3. **Study IOCP:** [IOCP Echo Server Architecture](IOCP_EchoServer_Architecture.md)
4. **Follow Flow:** [Echo Server Sequence Diagrams](EchoServer_Sequence_diagram.md)
5. **Build and Test:** [Building and Testing](BUILD_AND_TEST.md)

### For Developers

1. **API Reference:** [API Reference](API_REFERENCE.md)
2. **Network Layer:** [Network Layer Guide](NETWORK_LAYER.md)
3. **Error Handling:** [Error Handling Guide](ERROR_HANDLING.md)
4. **Configuration:** [Configuration Guide](CONFIGURATION.md)
5. **Code Style:** [Code Style Guide](CODE_STYLE.md)

### For Architects

1. **Architecture Overview:** [Architecture Overview](ARCHITECTURE.md)
2. **Protocol Design:** [Protocol Specification](PROTOCOL.md)
3. **Layer Separation:** [Architecture Overview](ARCHITECTURE.md#layer-separation)
4. **Threading Model:** [Network Layer Guide](NETWORK_LAYER.md#threading-model)

---

## 📖 Additional Resources

### External Documentation
- [Microsoft IOCP Documentation](https://docs.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
- [Winsock AcceptEx Reference](https://docs.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex)
- [FlatBuffers Documentation](https://google.github.io/flatbuffers/)

### Internal References
- [CLAUDE.md](../CLAUDE.md) - Claude Code guidance
- [README.md](../README.md) - Project overview

---

**Last Updated:** 2026-02-05
**Maintained By:** Project Team


# Architecture Overview

> **Last Updated:** 2026-02-05
> **Status:** Foundation Phase - Echo Server Implementation Complete

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Layer Separation](#layer-separation)
3. [Design Patterns](#design-patterns)
4. [Component Relationships](#component-relationships)
5. [Threading Model](#threading-model)
6. [Memory Management](#memory-management)
7. [Async I/O Flow](#async-io-flow)
8. [Critical Design Decisions](#critical-design-decisions)

---

## System Architecture

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Application Layer                          │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │ EchoServer  │  │  ChatServer  │  │ GameServer   │           │
│  │             │  │  (planned)   │  │  (planned)   │           │
│  └──────┬──────┘  └──────┬───────┘  └──────┬───────┘           │
│         │                │                  │                   │
│         │ implements IServerHandler         │                   │
│         └────────────────┼──────────────────┘                   │
└─────────────────────────┼──────────────────────────────────────┘
                          │
┌─────────────────────────┼──────────────────────────────────────┐
│                      Network Layer                              │
│         ┌───────────────▼─────────────────┐                     │
│         │    ServerLifeCycle              │                     │
│         │  (Lifecycle Management)         │                     │
│         └────┬──────────────────────┬─────┘                     │
│              │                      │                           │
│    ┌─────────▼────────┐   ┌────────▼──────────┐                │
│    │ IocpIoMultiplexer│   │  IocpAcceptor     │                │
│    │ (IOCP + Workers) │   │  (AcceptEx)       │                │
│    └─────────┬────────┘   └────────┬──────────┘                │
│              │                      │                           │
│              │         ┌────────────▼──────────┐                │
│              │         │      Client           │                │
│              │         │  (Connection State)   │                │
│              │         └───────────────────────┘                │
│              │                                                  │
│    ┌─────────▼──────────────────────────────────┐              │
│    │         IOCP Kernel Object                 │              │
│    │  (GetQueuedCompletionStatus)               │              │
│    └────────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────┼──────────────────────────────────────┐
│                    Utilities Layer                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │ Logger   │  │ Result<> │  │ObjectPool│  │  Errors  │       │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘       │
└─────────────────────────────────────────────────────────────────┘
```

### Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        EchoServer                               │
│  ┌──────────────────────────────────────────────────────┐       │
│  │  Responsibilities:                                   │       │
│  │  - Business logic (echo message back)                │       │
│  │  - Client lifecycle events (accept/recv/send/dc)     │       │
│  │  - Application-specific error handling               │       │
│  └──────────────────────────────────────────────────────┘       │
│                            │                                    │
│                            │ owns                               │
│                            ▼                                    │
│  ┌──────────────────────────────────────────────────────┐       │
│  │           ServerLifeCycle                            │       │
│  │  ┌────────────────────────────────────────────────┐  │       │
│  │  │  Responsibilities:                             │  │       │
│  │  │  - Server start/stop lifecycle                 │  │       │
│  │  │  - IOCP initialization                         │  │       │
│  │  │  - Acceptor initialization                     │  │       │
│  │  │  - Client pool management                      │  │       │
│  │  │  - I/O completion event routing                │  │       │
│  │  └────────────────────────────────────────────────┘  │       │
│  │                    │         │                        │       │
│  │                    │ owns    │ owns                   │       │
│  │        ┌───────────▼────┐   ┌▼──────────────┐        │       │
│  │        │IocpIoMultiplexer│   │ IocpAcceptor │        │       │
│  │        └───────────┬────┘   └┬──────────────┘        │       │
│  │                    │          │                       │       │
│  └────────────────────┼──────────┼───────────────────────┘       │
└───────────────────────┼──────────┼─────────────────────────────┘
                        │          │
            ┌───────────▼──┐  ┌────▼─────────┐
            │ Worker Thread│  │Accept Context│
            │   Pool       │  │  (per conn)  │
            └──────────────┘  └──────────────┘
```

---

## Layer Separation

### Layered Architecture Principles

| Layer | Location | Responsibilities | May NOT Access |
|-------|----------|-----------------|----------------|
| **Application** | `exec/echo-server/`, `exec/chat-server/` | Business logic, message processing, game rules | Winsock APIs, IOCP directly |
| **Network** | `network/` | Async I/O, IOCP, socket management, connection lifecycle | Application-specific logic |
| **Utilities** | `lib/` | Cross-cutting concerns: logging, error handling, memory pools | Application or network specifics |

### Dependency Rules

```
Application Layer
    ↓ depends on
Network Layer
    ↓ depends on
Utilities Layer
    ↓ depends on
Windows API / Standard Library
```

**Key Principle:** Higher layers may depend on lower layers, but never vice versa.

### Interface-Based Abstraction

#### IServerHandler Interface

```cpp
class IServerHandler {
public:
    virtual void OnAccept(std::shared_ptr<Client> client) = 0;
    virtual void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) = 0;
    virtual void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) = 0;
    virtual void OnDisconnect(std::shared_ptr<Client> client) = 0;
    virtual ~IServerHandler() = default;
};
```

**Purpose:** Decouples network layer from application-specific logic.

**Implementation:** `EchoServer` implements `IServerHandler` and is injected into `ServerLifeCycle`.

#### ISocket Interface

```cpp
class ISocket {
public:
    virtual SocketHandle GetHandle() const = 0;
    virtual bool IsValid() const = 0;
    // ... other socket operations
    virtual ~ISocket() = default;
};
```

**Purpose:** Abstracts socket implementation details.

**Implementation:** `WindowsAsyncSocket` implements `ISocket` using Winsock APIs.

---

## Design Patterns

### 1. Template Method Pattern

**Location:** `ServerLifeCycle` + `IServerHandler`

```cpp
class ServerLifeCycle {
    void OnCompletion(CompletionEvent event) {
        switch (event.ioType) {
            case EIoType::Accept:
                OnAcceptInternal(ctx);
                _handler->OnAccept(client);  // Template method callback
                break;
            case EIoType::Recv:
                HandleRecv(event);
                _handler->OnRecv(client, data);  // Template method callback
                break;
            // ...
        }
    }
};
```

**Benefit:** Network layer handles I/O mechanics; application provides business logic.

### 2. Object Pool Pattern

**Location:** `ObjectPool<T>` in `lib/ObjectPool.hpp`

```cpp
template<typename T>
class ObjectPool {
    T* Acquire();
    void Release(T* obj);
};

// Usage in IocpAcceptor
ObjectPool<AcceptOverlapped> _overlappedPool;
```

**Benefit:** Reduces allocation overhead for frequently created/destroyed objects (OVERLAPPED structures).

### 3. RAII (Resource Acquisition Is Initialization)

**Examples:**

- `std::jthread` for worker threads (auto-join on destruction)
- `std::shared_ptr<Client>` for connection lifetime
- `ServerLifeCycle::~ServerLifeCycle()` calls `Stop()`

**Benefit:** Automatic resource cleanup, exception safety.

### 4. Dependency Injection

**Example:** `ServerLifeCycle` constructor

```cpp
ServerLifeCycle(std::shared_ptr<Logger> logger, IServerHandler* handler);
```

**Benefit:** Loose coupling, testability, flexibility.

### 5. Result<T, E> Monad

**Location:** `lib/Result.hpp`

```cpp
Result<void, ENetworkError> Initialize(int workerThreadCount);

// Usage
auto res = iocp->Initialize(4);
if (!res) {
    log::Error("IOCP initialization failed: {}", ToString(res.Error()));
    return res;
}
```

**Benefit:** Explicit error handling, no exceptions, functional composition.

---

## Component Relationships

### Ownership Graph

```
EchoServer
  └─ owns unique_ptr<ServerLifeCycle>
       ├─ owns unique_ptr<IocpIoMultiplexer>
       │    └─ owns vector<jthread> (worker threads)
       │
       ├─ owns unique_ptr<IocpAcceptor>
       │    └─ owns ObjectPool<AcceptOverlapped>
       │
       └─ owns vector<shared_ptr<Client>>
            └─ shared between application and worker threads
```

### Callback Flow

```
Worker Thread (GQCS)
  ↓ calls
IocpIoMultiplexer::WorkerLoop()
  ↓ calls
_completionHandler (registered by ServerLifeCycle)
  ↓ calls
ServerLifeCycle::OnCompletion()
  ↓ dispatches to
  ├─ IocpAcceptor::OnAcceptComplete() → _acceptCallback
  │    ↓ calls
  │  ServerLifeCycle::OnAcceptInternal()
  │    ↓ calls
  │  IServerHandler::OnAccept() (implemented by EchoServer)
  │
  └─ ServerLifeCycle::HandleRecv()
       ↓ calls
     IServerHandler::OnRecv() (implemented by EchoServer)
```

### Data Flow: Echo Message

```
Client (remote)
  │ TCP send("Hello")
  ▼
Winsock Kernel (async recv)
  │ Completion posted to IOCP
  ▼
IOCP Kernel Object
  │ GetQueuedCompletionStatus() returns
  ▼
Worker Thread
  │ Calls completion handler
  ▼
ServerLifeCycle::HandleRecv()
  │ Extracts data from recvOverlapped.recvBuffer
  ▼
EchoServer::OnRecv(client, "Hello")
  │ Business logic: echo back
  ▼
Client::PostSend("Hello")
  │ WSASend() async
  ▼
IOCP Kernel Object
  │ Send completion
  ▼
ServerLifeCycle::HandleSend()
  │ Logging only
  ▼
Client (remote)
  │ TCP recv("Hello")
```

---

## Threading Model

### Thread Types

| Thread Type | Count | Role | Blocking Call |
|-------------|-------|------|---------------|
| **Main Thread** | 1 | Server initialization, shutdown coordination | `std::cin.get()` (wait for exit) |
| **Worker Threads** | N (configurable, typically # of CPU cores) | Process IOCP completions | `GetQueuedCompletionStatus()` |

### Worker Thread Lifecycle

```cpp
void IocpIoMultiplexer::WorkerLoop(std::stop_token st) {
    while (!st.stop_requested() && _isRunning) {
        DWORD bytes = 0;
        void* key = nullptr;
        LPOVERLAPPED overlapped = nullptr;

        BOOL success = GetQueuedCompletionStatus(
            _handle,
            &bytes,
            reinterpret_cast<PULONG_PTR>(&key),
            &overlapped,
            INFINITE  // Block indefinitely
        );

        // Check for shutdown signal
        if (overlapped == nullptr && key == nullptr) {
            break;  // Shutdown posted via PostQueuedCompletionStatus
        }

        // Construct CompletionEvent
        CompletionEvent event = { /* ... */ };

        // Invoke application callback
        if (_completionHandler) {
            _completionHandler(event);
        }
    }
}
```

### Concurrency Considerations

#### Thread-Safe Components

- `IocpIoMultiplexer` - IOCP handles concurrent GQCS calls
- `Client::PostRecv/PostSend` - Each client has separate recv/send OVERLAPPED
- `ObjectPool<T>` - Mutex-protected (if used from multiple threads)

#### Single-Threaded Components (by Design)

- `Client` - Each client is processed by one worker at a time (IOCP guarantees)
- `EchoServer::OnRecv` - No synchronization needed (per-client serialization)

#### Synchronization Points

- `ServerLifeCycle::_clientPool` - Protected by mutex when finding available slots
- `IocpIoMultiplexer::_isRunning` - `std::atomic<bool>` for thread-safe flag

---

## Memory Management

### Smart Pointer Strategy

| Resource | Pointer Type | Rationale |
|----------|--------------|-----------|
| **Client** | `std::shared_ptr<Client>` | Shared between application and worker threads; automatic cleanup when last reference drops |
| **ServerLifeCycle** | `std::unique_ptr<ServerLifeCycle>` | Single ownership by EchoServer |
| **IocpIoMultiplexer** | `std::unique_ptr<IocpIoMultiplexer>` | Single ownership by ServerLifeCycle |
| **IocpAcceptor** | `std::unique_ptr<IocpAcceptor>` | Single ownership by ServerLifeCycle |
| **Logger** | `std::shared_ptr<Logger>` | Shared across all components |

### Object Pool Usage

```cpp
// OverlappedExt structures pooled to avoid frequent allocation
ObjectPool<AcceptOverlapped> _overlappedPool;

// Acquisition
AcceptOverlapped* ovlp = _overlappedPool.Acquire();

// Usage
// ... AcceptEx async operation ...

// Release after completion
_overlappedPool.Release(ovlp);
```

### Memory Lifetime Guarantees

#### OVERLAPPED Structures

**Critical Requirement:** OVERLAPPED structures MUST remain valid until I/O completes.

**Solution:**

- Accept: Pooled in `IocpAcceptor::_overlappedPool`
- Recv/Send: Embedded in `Client::recvOverlapped` / `Client::sendOverlapped`

#### Client Lifetime

**Requirement:** Client must outlive pending I/O operations.

**Solution:** `std::shared_ptr<Client>` held by:

1. `ServerLifeCycle::_clientPool` (server-side reference)
2. `Client::enable_shared_from_this` (allows worker threads to hold temporary references)

**Cleanup:** When `Client::Close()` called, `shared_ptr` reference dropped after I/O completes.

---

## Async I/O Flow

### Connection Acceptance Flow

```
┌──────────────────────────────────────────────────────────────────┐
│  1. Server Initialization                                        │
├──────────────────────────────────────────────────────────────────┤
│  IocpAcceptor::Initialize()                                      │
│    ├─ CreateIoCompletionPort() associates listen socket to IOCP │
│    └─ WSAIoctl() loads AcceptEx function pointer                 │
└──────────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────────────┐
│  2. Post Accept Requests                                         │
├──────────────────────────────────────────────────────────────────┤
│  IocpAcceptor::PostAccepts(backlog)                              │
│    └─ For i = 0 to backlog:                                      │
│         ├─ Create accept socket (WSASocketW)                     │
│         ├─ Acquire AcceptOverlapped from pool                    │
│         └─ AcceptEx(listenSocket, acceptSocket, ovlp, ...)       │
│            [Non-blocking, returns immediately]                   │
└──────────────────────────────────────────────────────────────────┘
                          │
                          │ [Client connects]
                          ▼
┌──────────────────────────────────────────────────────────────────┐
│  3. Accept Completion (Worker Thread)                            │
├──────────────────────────────────────────────────────────────────┤
│  GetQueuedCompletionStatus() returns with Accept completion      │
│    ↓                                                             │
│  IocpIoMultiplexer::WorkerLoop()                                 │
│    ↓ calls                                                       │
│  ServerLifeCycle::OnCompletion(Accept)                           │
│    ↓ dispatches                                                  │
│  IocpAcceptor::OnAcceptComplete(ovlp)                            │
│    ├─ setsockopt(SO_UPDATE_ACCEPT_CONTEXT)                       │
│    ├─ GetAcceptExSockAddrs() [parse client address]             │
│    ├─ _acceptCallback(AcceptContext) → OnAcceptInternal()        │
│    │    ├─ Find available Client from pool                       │
│    │    ├─ client->socket = acceptSocket                         │
│    │    ├─ AssociateSocket(client->socket, client.get())         │
│    │    ├─ client->PostRecv()  [Start receiving]                 │
│    │    └─ _handler->OnAccept(client)  [Notify application]      │
│    └─ PostAccept()  [Repost for next connection]                 │
└──────────────────────────────────────────────────────────────────┘
```

### Recv/Send Flow

```
┌──────────────────────────────────────────────────────────────────┐
│  1. Post Recv                                                    │
├──────────────────────────────────────────────────────────────────┤
│  Client::PostRecv()                                              │
│    ├─ Initialize recvOverlapped.overlapped = {0}                 │
│    ├─ Set recvOverlapped.ioType = EIoType::Recv                  │
│    ├─ Setup WSABUF pointing to recvOverlapped.recvBuffer         │
│    └─ WSARecv(socket, &wsaBuf, ..., &recvOverlapped.overlapped) │
│       [Non-blocking, returns WSA_IO_PENDING]                     │
└──────────────────────────────────────────────────────────────────┘
                          │
                          │ [Data arrives]
                          ▼
┌──────────────────────────────────────────────────────────────────┐
│  2. Recv Completion (Worker Thread)                              │
├──────────────────────────────────────────────────────────────────┤
│  GetQueuedCompletionStatus() returns with Recv completion        │
│    ↓                                                             │
│  ServerLifeCycle::HandleRecv(event)                              │
│    ├─ Client* client = static_cast<Client*>(event.completionKey)│
│    ├─ data = client->recvOverlapped.recvBuffer[0..bytesRecv]    │
│    └─ _handler->OnRecv(client, data)  [Application callback]    │
│                                                                  │
│  EchoServer::OnRecv(client, data)                                │
│    ├─ [Business Logic: Echo back]                                │
│    ├─ client->PostSend(data)                                     │
│    └─ client->PostRecv()  [Continue receiving]                   │
└──────────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────────────────┐
│  3. Post Send                                                    │
├──────────────────────────────────────────────────────────────────┤
│  Client::PostSend(data)                                          │
│    ├─ Initialize sendOverlapped.overlapped = {0}                 │
│    ├─ Set sendOverlapped.ioType = EIoType::Send                  │
│    ├─ Copy data to sendOverlapped.sendBuffer                     │
│    ├─ Setup WSABUF pointing to sendOverlapped.sendBuffer         │
│    └─ WSASend(socket, &wsaBuf, ..., &sendOverlapped.overlapped) │
│       [Non-blocking, returns WSA_IO_PENDING]                     │
└──────────────────────────────────────────────────────────────────┘
                          │
                          │ [Send completes]
                          ▼
┌──────────────────────────────────────────────────────────────────┐
│  4. Send Completion (Worker Thread)                              │
├──────────────────────────────────────────────────────────────────┤
│  GetQueuedCompletionStatus() returns with Send completion        │
│    ↓                                                             │
│  ServerLifeCycle::HandleSend(event)                              │
│    └─ _handler->OnSend(client, bytesTransferred)                 │
│                                                                  │
│  EchoServer::OnSend(client, bytes)                               │
│    └─ [Logging only]                                             │
└──────────────────────────────────────────────────────────────────┘
```

---

## Critical Design Decisions

### 1. OverlappedExt First Member Invariant

**Decision:** `WSAOVERLAPPED overlapped` MUST be the first member of `OverlappedExt`.

**Rationale:**

```cpp
struct OverlappedExt {
    WSAOVERLAPPED overlapped;  // MUST BE FIRST!
    EIoType ioType;
    // ... other members
};

// In GQCS callback
LPOVERLAPPED lpOverlapped = /* from GQCS */;
OverlappedExt* ext = reinterpret_cast<OverlappedExt*>(lpOverlapped);
// Safe only if 'overlapped' is first member (zero offset)
```

**Consequence:** Cannot use inheritance (`struct OverlappedExt : WSAOVERLAPPED`) due to vtable pointer insertion.

### 2. Listen Socket IOCP Association

**Decision:** Listen socket MUST be associated with IOCP handle.

**Rationale:** `AcceptEx()` posts completion notifications to the IOCP associated with the **listen socket**, not the accept socket.

**Implementation:**

```cpp
IocpAcceptor::Initialize(SocketHandle listenSocket, HANDLE iocpHandle) {
    // Associate listen socket to IOCP
    auto res = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(listenSocket),
        iocpHandle,
        0,  // completionKey (unused for listen socket)
        0
    );
}
```

### 3. SO_UPDATE_ACCEPT_CONTEXT Requirement

**Decision:** Accepted sockets MUST have `SO_UPDATE_ACCEPT_CONTEXT` set immediately after `AcceptEx()` completes.

**Rationale:** Windows requirement for accepted sockets to inherit context from listen socket.

**Implementation:**

```cpp
setsockopt(
    acceptSocket,
    SOL_SOCKET,
    SO_UPDATE_ACCEPT_CONTEXT,
    reinterpret_cast<const char*>(&listenSocket),
    sizeof(listenSocket)
);
```

### 4. Separate Recv/Send OVERLAPPED

**Decision:** Each `Client` has separate `recvOverlapped` and `sendOverlapped`.

**Rationale:** Allows simultaneous recv and send operations on same socket.

**Implementation:**

```cpp
struct Client {
    RecvOverlapped recvOverlapped;  // Dedicated for WSARecv
    SendOverlapped sendOverlapped;  // Dedicated for WSASend
};
```

### 5. AcceptEx Indirect Invocation

**Decision:** Use `WSAIoctl()` to obtain `AcceptEx` function pointer instead of direct linking.

**Rationale:**

- **Portability:** Different Winsock service providers may implement AcceptEx differently.
- **Microsoft Recommendation:** Official documentation recommends indirect approach.

**Implementation:**

```cpp
GUID guidAcceptEx = WSAID_ACCEPTEX;
DWORD dwBytes = 0;
WSAIoctl(
    listenSocket,
    SIO_GET_EXTENSION_FUNCTION_POINTER,
    &guidAcceptEx,
    sizeof(guidAcceptEx),
    &_fnAcceptEx,
    sizeof(_fnAcceptEx),
    &dwBytes,
    nullptr,
    nullptr
);
```

### 6. Result<T, E> Over Exceptions

**Decision:** Use `Result<T, E>` monad instead of C++ exceptions for error handling.

**Rationale:**

- **Explicit Error Handling:** Compiler enforces error checking.
- **Performance:** No exception unwinding overhead in hot paths.
- **Functional Composition:** Enables error propagation patterns.

**Trade-off:** More verbose than exceptions, requires disciplined error handling.

### 7. Client Pool Pre-Allocation

**Decision:** Pre-allocate `std::vector<std::shared_ptr<Client>>` with max capacity.

**Rationale:**

- **Performance:** Avoid allocation in accept path.
- **Predictability:** Fixed memory footprint.

**Trade-off:** Wastes memory if max capacity not reached.

---

## Future Architecture Considerations

### Planned Enhancements

1. **Protocol Layer Integration**
   - FlatBuffers deserialization in recv path
   - Message routing based on `MessageType`

2. **Game State Management**
   - Separate game logic thread pool
   - Lockless message queues between network and game threads

3. **Database Integration Layer**
   - Async database operations (user persistence, inventory)
   - Connection pooling

4. **Metrics and Monitoring**
   - Performance counters (packets/sec, latency percentiles)
   - Health check endpoints

### Architectural Risks

| Risk | Mitigation |
|------|------------|
| **Single IOCP Bottleneck** | Profile under load; consider multiple IOCP instances if needed |
| **Client Pool Exhaustion** | Implement connection limits, queue management |
| **Large Recv Buffer Copies** | Investigate zero-copy techniques, buffer chaining |
| **Protocol Deserialization on Worker Thread** | Move to dedicated deserialization thread pool if bottleneck |

---

**See Also:**

- [Network Layer Guide](NETWORK_LAYER.md) for detailed IOCP mechanics
- [API Reference](API_REFERENCE.md) for class documentation
- [IOCP Echo Server Architecture](IOCP_EchoServer_Architecture.md) for implementation details

**[← Back to Index](INDEX.md)**

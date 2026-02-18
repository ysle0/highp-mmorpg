# Network Layer Documentation

## Table of Contents

1. [Introduction](#introduction)
2. [IOCP and Async I/O Concepts](#iocp-and-async-io-concepts)
3. [Architecture Overview](#architecture-overview)
4. [Core Classes](#core-classes)
   - [IocpIoMultiplexer](#iocpiomultiplexer)
   - [IocpAcceptor](#iocpacceptor)
   - [ServerLifeCycle](#serverlifecycle)
   - [Client](#client)
   - [OverlappedExt Structures](#overlappedext-structures)
5. [Threading Model and Concurrency](#threading-model-and-concurrency)
6. [Async I/O Patterns](#async-io-patterns)
7. [Error Handling](#error-handling)
8. [Code Examples](#code-examples)
9. [Performance Considerations](#performance-considerations)
10. [Best Practices](#best-practices)

---

## Introduction

The network layer provides a high-performance, scalable foundation for asynchronous I/O operations using Windows I/O Completion Ports (IOCP). This layer abstracts the complexity of IOCP and async socket operations, providing a clean interface for building network applications.

### Design Principles

- **Separation of Concerns**: Network layer handles I/O primitives, application layer handles business logic
- **Zero-Copy Where Possible**: Minimizes data copying through careful buffer management
- **Resource Pooling**: Pre-allocates resources to avoid runtime allocation overhead
- **Type Safety**: Uses strong typing and Result types for error handling
- **Thread Safety**: Lock-free designs and careful synchronization where needed

### Key Features

- IOCP-based async I/O multiplexing
- AcceptEx for high-performance connection acceptance
- Worker thread pool for completion event processing
- Object pooling for overlapped structures
- Graceful connection lifecycle management
- Configurable buffer sizes and thread counts

---

## IOCP and Async I/O Concepts

### What is IOCP?

I/O Completion Port (IOCP) is a Windows kernel mechanism for efficiently handling many concurrent I/O operations. Unlike traditional select/poll models, IOCP provides:

- **Thread Pool Management**: Kernel optimizes thread wake-ups based on CPU count
- **Completion-Based Model**: Threads are notified when I/O completes, not when ready
- **Scalability**: Handles thousands of concurrent connections efficiently
- **Fair Scheduling**: Kernel ensures fair distribution of completions across threads

### IOCP Workflow

```
1. Create IOCP Handle
   CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, workerCount)

2. Associate Socket with IOCP
   CreateIoCompletionPort(socketHandle, iocpHandle, completionKey, 0)

3. Initiate Async Operation
   WSARecv(socket, buffer, ..., overlapped, NULL)
   -> Returns immediately with ERROR_IO_PENDING

4. Wait for Completion (in worker thread)
   GetQueuedCompletionStatus(iocpHandle, &bytes, &key, &overlapped, INFINITE)
   -> Blocks until I/O completes

5. Process Completion
   - Extract completion key (identifies which connection)
   - Extract overlapped (contains I/O context)
   - Extract bytes transferred
   - Handle success/failure
```

### Key IOCP Concepts

**Completion Key**: A user-defined pointer passed during socket association. In this implementation, it's the `Client*` pointer, allowing immediate identification of which client the I/O operation belongs to.

**Overlapped Structure**: Contains context for the async operation. Extended to `OverlappedExt` to include I/O type, buffers, and other metadata.

**Worker Threads**: Pool of threads blocked on `GetQueuedCompletionStatus()`. The kernel wakes threads as I/O operations complete, managing concurrency automatically.

### Async I/O Model

```
Application                 Windows Kernel              Hardware
    |                            |                         |
    | WSARecv()                  |                         |
    |--------------------------->|                         |
    | (returns immediately)      |                         |
    |<---------------------------|                         |
    |                            |                         |
    | [Application continues]    | [Waits for data]        |
    |                            |------------------------>|
    |                            |                         |
    |                            |<------------------------|
    | GetQueuedCompletionStatus()|   [Data arrives]        |
    | (worker wakes up)          |                         |
    |<---------------------------|                         |
    | [Process data]             |                         |
```

### AcceptEx vs accept()

Traditional `accept()` is synchronous and blocks. `AcceptEx` is asynchronous:

```cpp
// Traditional (blocking)
SOCKET clientSocket = accept(listenSocket, ...);  // Blocks here!

// AcceptEx (non-blocking)
SOCKET acceptSocket = WSASocket(...);  // Pre-create socket
AcceptEx(listenSocket, acceptSocket, buffer, ..., overlapped);  // Returns immediately
// Completion notification arrives via GetQueuedCompletionStatus()
```

**Benefits of AcceptEx**:
- Non-blocking: Server thread pool handles multiple accepts simultaneously
- First data optimization: Can receive initial client data in same call
- Address retrieval: Gets local/remote addresses via `GetAcceptExSockAddrs`

---

## Architecture Overview

### Layer Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│  (EchoServer, ChatServer, GameServer - Business Logic)       │
├──────────────────────────────────────────────────────────────┤
│                  IServerHandler Interface                    │
│     (OnAccept, OnRecv, OnSend, OnDisconnect callbacks)       │
├──────────────────────────────────────────────────────────────┤
│                    ServerLifeCycle                           │
│        (Orchestrates components, manages lifecycle)          │
├───────────────┬──────────────────────┬───────────────────────┤
│               │                      │                       │
│  IocpIoMultiplexer  IocpAcceptor     │   Client Pool         │
│  (IOCP + Workers)   (AcceptEx)       │   (Connection State)  │
│               │                      │                       │
├───────────────┴──────────────────────┴───────────────────────┤
│              Windows IOCP & Winsock API                      │
└──────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Layer |
|-----------|----------------|-------|
| **IocpIoMultiplexer** | IOCP handle management, worker thread pool | Network |
| **IocpAcceptor** | AcceptEx-based connection acceptance | Network |
| **ServerLifeCycle** | Component orchestration, lifecycle management | Network |
| **Client** | Per-connection state, I/O buffers, WSARecv/WSASend | Network |
| **OverlappedExt** | Async I/O context (extends WSAOVERLAPPED) | Network |
| **IServerHandler** | Business logic interface (implemented by app) | Application |

### Data Flow

```
New Connection
    │
    ├─> AcceptEx completes
    │
    ├─> IocpAcceptor::OnAcceptComplete()
    │   ├─> Set SO_UPDATE_ACCEPT_CONTEXT
    │   ├─> Parse addresses via GetAcceptExSockAddrs
    │   └─> Invoke AcceptCallback
    │
    ├─> ServerLifeCycle::OnAcceptInternal()
    │   ├─> Find available Client from pool
    │   ├─> Associate socket with IOCP
    │   ├─> Client::PostRecv() (start receiving)
    │   └─> IServerHandler::OnAccept() (notify app)
    │
    └─> IocpAcceptor::PostAccept() (accept next connection)

Data Reception
    │
    ├─> WSARecv completes
    │
    ├─> Worker thread wakes from GetQueuedCompletionStatus()
    │
    ├─> IocpIoMultiplexer::WorkerLoop()
    │   └─> Invoke CompletionHandler
    │
    ├─> ServerLifeCycle::OnCompletion(Recv event)
    │   ├─> Extract Client* from completionKey
    │   ├─> Extract data from RecvOverlapped
    │   └─> IServerHandler::OnRecv() (notify app)
    │
    └─> Application processes and optionally calls Client::PostSend()
```

---

## Core Classes

### IocpIoMultiplexer

**Location**: `network/IocpIoMultiplexer.h`, `network/IocpIoMultiplexer.cpp`

**Purpose**: Manages the IOCP kernel object and worker thread pool. Provides the foundation for all async I/O operations.

#### Key Responsibilities

1. Create and manage IOCP handle
2. Maintain worker thread pool
3. Dispatch completion events to registered handlers
4. Clean shutdown with thread synchronization

#### Public Interface

```cpp
class IocpIoMultiplexer final {
public:
    using Res = fn::Result<void, err::ENetworkError>;

    // Constructor: Initialize with logger and optional completion handler
    explicit IocpIoMultiplexer(
        std::shared_ptr<log::Logger> logger,
        CompletionHandler handler = nullptr);

    // Create IOCP and start worker threads
    Res Initialize(int workerThreadCount);

    // Stop all workers and close IOCP handle
    void Shutdown();

    // Associate a socket with IOCP for completion notifications
    Res AssociateSocket(SocketHandle socket, void* completionKey);

    // Manually post a completion event (used for shutdown signaling)
    Res PostCompletion(DWORD bytes, void* key, LPOVERLAPPED overlapped);

    // Register completion event handler
    void SetCompletionHandler(CompletionHandler handler);

    // Get IOCP handle (for IocpAcceptor initialization)
    HANDLE GetHandle() const noexcept;

    // Check if IOCP is running
    bool IsRunning() const noexcept;
};
```

#### CompletionEvent Structure

```cpp
struct CompletionEvent {
    EIoType ioType;              // Accept, Recv, Send, Disconnect
    void* completionKey;          // Client* pointer
    LPOVERLAPPED overlapped;      // Points to OverlappedExt
    DWORD bytesTransferred;       // Bytes sent/received
    bool success;                 // I/O operation succeeded
    DWORD errorCode;              // GetLastError() if failed
};
```

#### Usage Pattern

```cpp
// 1. Create and initialize
auto iocp = std::make_unique<IocpIoMultiplexer>(
    logger,
    [this](CompletionEvent event) { OnCompletion(event); }
);

int workerCount = std::thread::hardware_concurrency() * 2;
auto res = iocp->Initialize(workerCount);
if (res.HasErr()) {
    // Handle error
}

// 2. Associate client sockets
iocp->AssociateSocket(clientSocket, clientPtr);

// 3. Shutdown (automatic in destructor)
iocp->Shutdown();
```

#### Worker Thread Implementation

```cpp
void IocpIoMultiplexer::WorkerLoop(std::stop_token st) {
    while (!st.stop_requested() && _isRunning.load()) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED overlapped = nullptr;

        // Block until I/O completes or shutdown signal
        BOOL ok = GetQueuedCompletionStatus(
            _handle,
            &bytesTransferred,
            &completionKey,
            &overlapped,
            INFINITE);

        // Shutdown signal (key=0, overlapped=nullptr)
        if (completionKey == 0 && overlapped == nullptr) {
            break;
        }

        // Build completion event
        CompletionEvent event{
            .completionKey = reinterpret_cast<void*>(completionKey),
            .overlapped = overlapped,
            .bytesTransferred = bytesTransferred,
            .success = (ok == TRUE),
            .errorCode = (ok == FALSE) ? GetLastError() : 0,
        };

        // Extract I/O type from overlapped extension
        if (overlapped != nullptr) {
            auto* ext = reinterpret_cast<OverlappedExt*>(overlapped);
            event.ioType = ext->ioType;
        }

        // Invoke registered handler
        if (_completionHandler) {
            _completionHandler(event);
        }
    }
}
```

#### Thread Safety Notes

- `_isRunning`: Atomic flag for shutdown coordination
- `_completionHandler`: Set once during initialization, no locking needed
- IOCP handle: Thread-safe by design (kernel object)
- `std::jthread`: Automatically joins on destruction, RAII-safe

---

### IocpAcceptor

**Location**: `network/IocpAcceptor.h`, `network/IocpAcceptor.cpp`

**Purpose**: Handles asynchronous connection acceptance using `AcceptEx`. Manages accept socket creation and overlapped structure pooling.

#### Key Responsibilities

1. Load `AcceptEx` and `GetAcceptExSockAddrs` function pointers via `WSAIoctl`
2. Pre-post multiple accept operations (backlog size)
3. Process accept completions and parse client addresses
4. Automatically re-post accepts for continuous listening
5. Pool overlapped structures for reuse

#### Public Interface

```cpp
class IocpAcceptor final {
public:
    using Res = fn::Result<void, err::ENetworkError>;

    // Constructor: Initialize with logger and optional callback
    explicit IocpAcceptor(
        std::shared_ptr<log::Logger> logger,
        std::shared_ptr<SocketOptionBuilder> socketOptionBuilder = nullptr,
        int preAllocCount = 10,
        AcceptCallback onAfterAccept = nullptr);

    // Associate listen socket with IOCP and load AcceptEx functions
    Res Initialize(SocketHandle listenSocket, HANDLE iocpHandle);

    // Post a single AcceptEx operation
    Res PostAccept();

    // Post multiple AcceptEx operations (backlog count)
    Res PostAccepts(int count);

    // Handle accept completion (called by worker thread)
    Res OnAcceptComplete(AcceptOverlapped* overlapped, DWORD bytesTransferred);

    // Register accept callback
    void SetAcceptCallback(AcceptCallback callback);

    // Shutdown and cleanup
    void Shutdown();
};
```

#### AcceptContext Structure

```cpp
struct AcceptContext {
    SocketHandle acceptSocket;    // Newly accepted client socket
    SocketHandle listenSocket;    // Server listen socket
    SOCKADDR_IN localAddr;        // Server address
    SOCKADDR_IN remoteAddr;       // Client address
};
```

#### AcceptEx Function Loading

AcceptEx is not directly linked but loaded at runtime for service provider independence:

```cpp
Res IocpAcceptor::LoadAcceptExFunctions() {
    GUID guidAcceptEx = WSAID_ACCEPTEX;
    DWORD bytes = 0;

    int result = WSAIoctl(
        _listenSocket,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidAcceptEx,
        sizeof(guidAcceptEx),
        &_fnAcceptEx,           // Function pointer output
        sizeof(_fnAcceptEx),
        &bytes,
        nullptr,
        nullptr);

    // Similar for GetAcceptExSockAddrs...
}
```

#### Accept Workflow

```cpp
Res IocpAcceptor::PostAccept() {
    // 1. Get overlapped from pool
    AcceptOverlapped* overlapped = _overlappedPool.Acquire();

    // 2. Create accept socket (pre-created for AcceptEx)
    SocketHandle acceptSocket = WSASocketW(
        AF_INET, SOCK_STREAM, IPPROTO_TCP,
        nullptr, 0, WSA_FLAG_OVERLAPPED);

    // 3. Initialize overlapped
    ZeroMemory(&overlapped->overlapped, sizeof(WSAOVERLAPPED));
    overlapped->ioType = EIoType::Accept;
    overlapped->clientSocket = acceptSocket;

    // 4. Call AcceptEx
    constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;
    BOOL result = _fnAcceptEx(
        _listenSocket,
        acceptSocket,
        overlapped->buf,        // Buffer for addresses
        0,                      // Don't receive initial data
        addrLen,                // Local address length
        addrLen,                // Remote address length
        &bytesReceived,
        reinterpret_cast<LPOVERLAPPED>(overlapped));

    // 5. Check for immediate completion or pending
    if (result == FALSE && WSAGetLastError() != ERROR_IO_PENDING) {
        // Error occurred
        closesocket(acceptSocket);
        _overlappedPool.Release(overlapped);
        return Res::Err(err::ENetworkError::SocketPostAcceptFailed);
    }

    return Res::Ok();
}
```

#### Accept Completion Handling

```cpp
Res IocpAcceptor::OnAcceptComplete(AcceptOverlapped* overlapped, DWORD bytesTransferred) {
    // 1. Update accept context (required for proper socket function)
    setsockopt(
        overlapped->clientSocket,
        SOL_SOCKET,
        SO_UPDATE_ACCEPT_CONTEXT,
        reinterpret_cast<char*>(&_listenSocket),
        sizeof(_listenSocket));

    // 2. Parse addresses from buffer
    SOCKADDR_IN* localAddr = nullptr;
    SOCKADDR_IN* remoteAddr = nullptr;
    int localAddrLen = 0;
    int remoteAddrLen = 0;
    constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;

    _fnGetAcceptExSockAddrs(
        overlapped->buf,
        0,                  // We didn't receive data
        addrLen,
        addrLen,
        reinterpret_cast<SOCKADDR**>(&localAddr),
        &localAddrLen,
        reinterpret_cast<SOCKADDR**>(&remoteAddr),
        &remoteAddrLen);

    // 3. Build context and invoke callback
    if (_acceptCallback) {
        AcceptContext ctx;
        ctx.acceptSocket = overlapped->clientSocket;
        ctx.listenSocket = _listenSocket;
        if (localAddr) ctx.localAddr = *localAddr;
        if (remoteAddr) ctx.remoteAddr = *remoteAddr;

        _acceptCallback(ctx);
    }

    // 4. Return overlapped to pool
    _overlappedPool.Release(overlapped);

    // 5. Re-post accept for next connection
    return PostAccept();
}
```

#### Critical Requirements

1. **Listen Socket IOCP Association**: The listen socket must be associated with IOCP to receive accept completions:
   ```cpp
   CreateIoCompletionPort(
       reinterpret_cast<HANDLE>(_listenSocket),
       _iocpHandle,
       0,  // Completion key not needed for listen socket
       0);
   ```

2. **SO_UPDATE_ACCEPT_CONTEXT**: Must be called on accepted socket before use:
   ```cpp
   setsockopt(acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, ...);
   ```

3. **Pre-Created Socket**: AcceptEx requires the accept socket to be created beforehand, unlike traditional `accept()`.

---

### ServerLifeCycle

**Location**: `network/ServerLifecycle.h`, `network/ServerLifecycle.cpp`

**Purpose**: Orchestrates all network components and manages server lifecycle. Acts as the bridge between low-level network operations and application business logic.

#### Key Responsibilities

1. Initialize and coordinate IocpIoMultiplexer and IocpAcceptor
2. Manage client pool and connection lifecycle
3. Route completion events to appropriate handlers
4. Bridge network events to IServerHandler callbacks
5. Handle graceful shutdown

#### Public Interface

```cpp
class ServerLifeCycle final {
public:
    using Res = fn::Result<void, err::ENetworkError>;

    // Constructor: Initialize with logger and handler
    explicit ServerLifeCycle(
        std::shared_ptr<log::Logger> logger,
        IServerHandler* handler);

    // Start server with listen socket and configuration
    Res Start(std::shared_ptr<ISocket> listenSocket, const NetworkCfg& config);

    // Stop server and cleanup all resources
    void Stop();

    // Close a specific client connection
    void CloseClient(std::shared_ptr<Client> client, bool force = true);

    // Get current connected client count
    size_t GetConnectedClientCount() const noexcept;

    // Check if server is running
    bool IsRunning() const noexcept;
};
```

#### Initialization Sequence

```cpp
Res ServerLifeCycle::Start(std::shared_ptr<ISocket> listenSocket, const NetworkCfg& config) {
    _config = config;

    // 1. Initialize IOCP with completion handler
    _iocp = std::make_unique<IocpIoMultiplexer>(
        _logger,
        std::bind_front(&ServerLifeCycle::OnCompletion, this));

    const auto workerCount = std::thread::hardware_concurrency() *
        _config.thread.maxWorkerThreadMultiplier;

    if (auto res = _iocp->Initialize(workerCount); res.HasErr()) {
        return res;
    }

    // 2. Initialize Acceptor with accept callback
    _acceptor = std::make_unique<IocpAcceptor>(
        _logger,
        _config.server.backlog,
        std::bind_front(&ServerLifeCycle::OnAcceptInternal, this));

    if (auto res = _acceptor->Initialize(
            listenSocket->GetSocketHandle(),
            _iocp->GetHandle()); res.HasErr()) {
        return res;
    }

    // 3. Pre-allocate client pool
    _clientPool.reserve(_config.server.maxClients);
    for (int i = 0; i < _config.server.maxClients; ++i) {
        _clientPool.emplace_back(std::make_shared<Client>());
    }

    // 4. Post initial accept operations
    if (auto res = _acceptor->PostAccepts(_config.server.backlog); res.HasErr()) {
        return res;
    }

    _logger->Info("Server started on port {}.", _config.server.port);
    return Res::Ok();
}
```

#### Completion Event Routing

```cpp
void ServerLifeCycle::OnCompletion(CompletionEvent event) {
    switch (event.ioType) {
        case EIoType::Accept: {
            auto* overlapped = reinterpret_cast<AcceptOverlapped*>(event.overlapped);
            if (_acceptor) {
                _acceptor->OnAcceptComplete(overlapped, event.bytesTransferred);
            }
            break;
        }

        case EIoType::Recv:
            HandleRecv(event);
            break;

        case EIoType::Send:
            HandleSend(event);
            break;

        default:
            _logger->Error("Unknown IO type received.");
            break;
    }
}
```

#### Accept Handling

```cpp
void ServerLifeCycle::OnAcceptInternal(AcceptContext& ctx) {
    // 1. Find available client slot
    auto client = FindAvailableClient();
    if (!client) {
        _logger->Error("Client pool full!");
        closesocket(ctx.acceptSocket);
        return;
    }

    // 2. Assign socket to client
    client->socket = ctx.acceptSocket;

    // 3. Associate with IOCP (completion key = Client*)
    if (auto res = _iocp->AssociateSocket(client->socket, client.get());
        res.HasErr()) {
        _logger->Error("Failed to associate socket with IOCP.");
        client->Close(true);
        return;
    }

    // 4. Start receiving data
    if (auto res = client->PostRecv(); res.HasErr()) {
        _logger->Error("Failed to post initial Recv.");
        client->Close(true);
        return;
    }

    // 5. Update connection count
    _connectedClientCount++;

    // 6. Log connection
    char clientIp[Const::Buffer::clientIpBufferSize]{ 0 };
    inet_ntop(AF_INET, &ctx.remoteAddr.sin_addr, clientIp, sizeof(clientIp));
    _logger->Info("Client connected. socket: {}, ip: {}", client->socket, clientIp);

    // 7. Notify application layer
    if (_handler) {
        _handler->OnAccept(client);
    }
}
```

#### Receive Handling

```cpp
void ServerLifeCycle::HandleRecv(CompletionEvent& event) {
    auto* client = static_cast<Client*>(event.completionKey);
    if (!client) return;

    auto clientPtr = client->shared_from_this();

    // Detect graceful disconnect (0 bytes received or failure)
    if (!event.success || event.bytesTransferred == 0) {
        _logger->Info("[Graceful Disconnect] Socket #{} disconnected.", client->socket);
        if (_handler) {
            _handler->OnDisconnect(clientPtr);
        }
        CloseClient(clientPtr, true);
        return;
    }

    // Extract data from receive buffer
    auto* overlapped = reinterpret_cast<RecvOverlapped*>(event.overlapped);
    std::span<const char> data{ overlapped->buf, event.bytesTransferred };

    // Notify application layer
    if (_handler) {
        _handler->OnRecv(clientPtr, data);
    }
}
```

#### Send Handling

```cpp
void ServerLifeCycle::HandleSend(CompletionEvent& event) {
    auto* client = static_cast<Client*>(event.completionKey);
    if (!client) return;

    auto clientPtr = client->shared_from_this();

    // Notify application layer
    if (_handler) {
        _handler->OnSend(clientPtr, event.bytesTransferred);
    }
}
```

#### Client Pool Management

```cpp
std::shared_ptr<Client> ServerLifeCycle::FindAvailableClient() {
    auto found = std::find_if(_clientPool.begin(), _clientPool.end(),
        [](const std::shared_ptr<Client>& c) {
            return c->socket == INVALID_SOCKET;
        });

    if (found != _clientPool.end()) {
        return *found;
    }
    return nullptr;
}
```

---

### Client

**Location**: `network/Client.h`, `network/Client.cpp`

**Purpose**: Represents a single client connection. Manages connection state, I/O buffers, and provides async send/receive operations.

#### Key Responsibilities

1. Maintain connection state (socket handle)
2. Manage per-connection I/O buffers (recv/send overlapped structures)
3. Provide async receive operation (WSARecv)
4. Provide async send operation (WSASend)
5. Handle connection closure

#### Class Definition

```cpp
struct Client : public std::enable_shared_from_this<Client> {
    using Res = fn::Result<void, err::ENetworkError>;

    // Constructor: Initialize buffers and socket
    Client();

    // Initiate async receive operation
    Res PostRecv();

    // Initiate async send operation
    Res PostSend(std::string_view data);

    // Close connection gracefully or forcefully
    void Close(bool isFireAndForget);

    // Connection state
    SocketHandle socket = INVALID_SOCKET;

    // I/O buffers (one per operation type)
    RecvOverlapped recvOverlapped;
    SendOverlapped sendOverlapped;
};
```

#### Async Receive

```cpp
Client::Res Client::PostRecv() {
    // 1. Zero overlapped structure (required for reuse)
    ZeroMemory(&recvOverlapped.overlapped, sizeof(WSAOVERLAPPED));

    // 2. Setup buffer descriptor
    recvOverlapped.bufDesc.buf = recvOverlapped.buf;
    recvOverlapped.bufDesc.len = std::size(recvOverlapped.buf);
    recvOverlapped.ioType = EIoType::Recv;

    // 3. Initiate async receive
    DWORD flags = 0;
    DWORD recvNumBytes = 0;

    int result = WSARecv(
        socket,
        &recvOverlapped.bufDesc,
        1,                      // Buffer count
        &recvNumBytes,
        &flags,
        reinterpret_cast<LPWSAOVERLAPPED>(&recvOverlapped),
        nullptr);               // No completion routine (using IOCP)

    // 4. Check result (ERROR_IO_PENDING is expected)
    if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
        return Res::Err(err::ENetworkError::WsaRecvFailed);
    }

    return Res::Ok();
}
```

#### Async Send

```cpp
Client::Res Client::PostSend(std::string_view data) {
    // 1. Zero overlapped structure
    ZeroMemory(&sendOverlapped.overlapped, sizeof(WSAOVERLAPPED));

    // 2. Copy data to send buffer
    auto len = static_cast<ULONG>(data.size());
    CopyMemory(sendOverlapped.buf, data.data(), len);

    // 3. Setup buffer descriptor
    sendOverlapped.bufDesc.buf = sendOverlapped.buf;
    sendOverlapped.bufDesc.len = len;
    sendOverlapped.ioType = EIoType::Send;

    // 4. Initiate async send
    DWORD sendNumBytes = 0;

    int result = WSASend(
        socket,
        &sendOverlapped.bufDesc,
        1,                      // Buffer count
        &sendNumBytes,
        0,                      // Flags
        reinterpret_cast<LPWSAOVERLAPPED>(&sendOverlapped),
        nullptr);               // No completion routine (using IOCP)

    // 5. Check result
    if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
        return Res::Err(err::ENetworkError::WsaSendFailed);
    }

    return Res::Ok();
}
```

#### Connection Closure

```cpp
void Client::Close(bool isFireAndForget) {
    // Optionally set linger for fire-and-forget (immediate close)
    // Currently commented out for graceful shutdown preference

    // Shutdown both send and receive
    shutdown(socket, SD_BOTH);

    // Close socket handle
    closesocket(socket);

    // Mark as closed
    socket = INVALID_SOCKET;
}
```

#### Why std::enable_shared_from_this?

Clients are stored in `std::shared_ptr` in the pool. When completion events occur, we have a raw `Client*` (from completion key). To safely pass to application callbacks, we need to obtain a `shared_ptr`:

```cpp
void ServerLifeCycle::HandleRecv(CompletionEvent& event) {
    auto* client = static_cast<Client*>(event.completionKey);

    // Convert raw pointer to shared_ptr safely
    auto clientPtr = client->shared_from_this();

    // Now safe to pass to application
    _handler->OnRecv(clientPtr, data);
}
```

#### Buffer Size Configuration

Buffer sizes are configured in `network/Const.h` (auto-generated from `config.compile.toml`):

```cpp
struct Const {
    struct Buffer {
        static constexpr INT recvBufferSize = 4096;  // Receive buffer
        static constexpr INT sendBufferSize = 1024;  // Send buffer
    };
};
```

---

### OverlappedExt Structures

**Location**: `network/OverlappedExt.h`

**Purpose**: Extended OVERLAPPED structures that add I/O type identification, buffer storage, and additional context for async operations.

#### Critical Memory Layout Requirement

The `WSAOVERLAPPED` member **must** be the first member of each structure. This enables safe casting:

```cpp
// Safe because overlapped is first member
OverlappedExt* ext = reinterpret_cast<OverlappedExt*>(lpOverlapped);
```

If `WSAOVERLAPPED` is not first, the pointer arithmetic would be incorrect, causing memory corruption.

#### SendOverlapped

```cpp
struct SendOverlapped {
    WSAOVERLAPPED overlapped;        // MUST be first!
    EIoType ioType = EIoType::Send;
    SocketHandle clientSocket = InvalidSocket;
    WSABUF bufDesc;
    char buf[Const::Buffer::sendBufferSize];
};
```

**Usage**: Attached to `WSASend()` calls. When send completes, worker extracts this structure to identify the operation and access sent data.

#### RecvOverlapped

```cpp
struct RecvOverlapped {
    WSAOVERLAPPED overlapped;        // MUST be first!
    EIoType ioType = EIoType::Recv;
    SocketHandle clientSocket = InvalidSocket;
    WSABUF bufDesc;
    char buf[Const::Buffer::recvBufferSize];
};
```

**Usage**: Attached to `WSARecv()` calls. When receive completes, worker extracts received data from `buf`.

#### AcceptOverlapped

```cpp
struct AcceptOverlapped {
    WSAOVERLAPPED overlapped;        // MUST be first!
    EIoType ioType = EIoType::Accept;
    SocketHandle clientSocket = InvalidSocket;
    WSABUF bufDesc;
    char buf[Const::Buffer::addressBufferSize];
};
```

**Usage**: Attached to `AcceptEx()` calls. Buffer holds local and remote socket addresses (parsed via `GetAcceptExSockAddrs`).

#### Why Separate Structures?

1. **Type Safety**: Each I/O type has appropriate buffer size
2. **Clear Intent**: Explicit structure per operation type
3. **Pooling**: Can pool each type separately with appropriate sizes
4. **Debugging**: Easier to inspect and validate in debugger

#### WSABUF Setup Pattern

```cpp
// Setup for receive
recvOverlapped.bufDesc.buf = recvOverlapped.buf;
recvOverlapped.bufDesc.len = sizeof(recvOverlapped.buf);

// Pass to WSARecv
WSARecv(socket, &recvOverlapped.bufDesc, 1, ...);
```

The `WSABUF` structure tells Winsock where to read/write data:
- `.buf`: Pointer to buffer
- `.len`: Buffer length in bytes

#### EIoType Enumeration

```cpp
enum class EIoType {
    Accept,      // AcceptEx operation
    Recv,        // WSARecv operation
    Send,        // WSASend operation
    Disconnect   // Graceful disconnect
};
```

Used to route completion events to appropriate handlers in `ServerLifeCycle::OnCompletion()`.

---

## Threading Model and Concurrency

### Thread Pool Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         IOCP Kernel Object                      │
│                     (Completion Queue)                          │
└────────────────────┬────────────────────────────────────────────┘
                     │
         ┌───────────┼───────────┬───────────┬───────────┐
         │           │           │           │           │
    ┌────▼────┐ ┌────▼────┐ ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
    │Worker 1 │ │Worker 2 │ │Worker 3 │ │Worker 4 │ │Worker N │
    └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘
         │           │           │           │           │
         │ GQCS()    │ GQCS()    │ GQCS()    │ GQCS()    │ GQCS()
         │ blocks    │ blocks    │ blocks    │ blocks    │ blocks
         │           │           │           │           │
         └───────────┴───────────┴───────────┴───────────┘
                     │
                     ▼
         ┌────────────────────────┐
         │  CompletionHandler     │
         │  (Application Logic)   │
         └────────────────────────┘
```

### Worker Thread Count

**Formula**: `CPU_CORES * maxWorkerThreadMultiplier`

**Default Configuration**:
```toml
[thread]
max_worker_thread_multiplier = 2
```

**Rationale**:
- 1x CPU cores: Good for CPU-bound workloads
- 2x CPU cores: Better for I/O-bound workloads (recommended)
- More than 2x: Diminishing returns, increased context switching

**Example**:
- 8-core CPU with 2x multiplier = 16 worker threads
- Each thread blocks on `GetQueuedCompletionStatus()`
- Kernel wakes threads as I/O completes

### Concurrency Considerations

#### IOCP Thread Safety

IOCP is inherently thread-safe:
- Multiple threads can call `GetQueuedCompletionStatus()` on same handle
- Kernel ensures each completion is delivered to exactly one thread
- No user-level locking required for IOCP operations

#### Client Concurrency Model

**Per-Client Serialization**: Each client has separate recv/send overlapped structures:

```cpp
struct Client {
    RecvOverlapped recvOverlapped;  // Only one recv at a time
    SendOverlapped sendOverlapped;  // Only one send at a time
};
```

**Implications**:
- One pending receive per client at any time
- One pending send per client at any time
- Receive and send can overlap (different operations)
- Multiple receives on same client would require overlapped array

#### Application Handler Concurrency

**Critical**: The `IServerHandler` callbacks are invoked from worker threads:

```cpp
void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
    // This runs in worker thread context!
    // Multiple workers may invoke this simultaneously for different clients
    // Must be thread-safe if accessing shared state
}
```

**Thread-Safety Requirements**:
- Per-client state: No locking needed (one worker handles one client's event)
- Shared server state: Requires synchronization (mutex, atomics, etc.)
- Logger calls: Assumed thread-safe (spdlog is thread-safe by default)

#### Atomic Operations

```cpp
class ServerLifeCycle {
    std::atomic<size_t> _connectedClientCount{ 0 };  // Thread-safe counter
};

// Safe from any thread
_connectedClientCount++;
_connectedClientCount--;
```

#### Lock-Free Client Pool

The client pool is pre-allocated and uses `INVALID_SOCKET` as availability flag:

```cpp
std::shared_ptr<Client> FindAvailableClient() {
    // Linear search for available slot
    auto found = std::find_if(_clientPool.begin(), _clientPool.end(),
        [](const std::shared_ptr<Client>& c) {
            return c->socket == INVALID_SOCKET;
        });
    return found != _clientPool.end() ? *found : nullptr;
}
```

**Race Condition**: Two workers could find and assign same client simultaneously. **Mitigation** (not currently implemented): Use `std::atomic<SocketHandle>` or introduce accept lock.

### Shutdown Synchronization

```cpp
void IocpIoMultiplexer::Shutdown() {
    // 1. Set shutdown flag
    if (!_isRunning.exchange(false)) {
        return;  // Already shut down
    }

    // 2. Request thread stops and post wake-up signals
    for (auto& worker : _workerThreads) {
        worker.request_stop();
        PostQueuedCompletionStatus(_handle, 0, 0, nullptr);
    }

    // 3. Clear threads (std::jthread auto-joins)
    _workerThreads.clear();

    // 4. Close IOCP handle
    if (_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(_handle);
        _handle = INVALID_HANDLE_VALUE;
    }
}
```

**std::jthread Benefits**:
- Destructor automatically joins
- `request_stop()` sets stop token
- RAII-safe: No manual join management

---

## Async I/O Patterns

### Pattern 1: Accept-Recv-Send Loop

The fundamental pattern for connection-oriented servers:

```
┌──────────────────────────────────────────────────────────────┐
│                    Accept-Recv-Send Loop                     │
└──────────────────────────────────────────────────────────────┘

    ┌─────────────┐
    │ PostAccept  │  (Pre-post backlog count)
    └──────┬──────┘
           │
           ▼
    ┌─────────────┐
    │   AcceptEx  │  (Async, returns immediately)
    └──────┬──────┘
           │
           ▼
    ┌──────────────────┐
    │ GQCS (blocked)   │
    └──────┬───────────┘
           │ [Client connects]
           ▼
    ┌──────────────────┐
    │ OnAcceptComplete │
    └──────┬───────────┘
           │
           ├─> Associate socket with IOCP
           ├─> PostRecv() (start receiving)
           └─> PostAccept() (accept next)

           ┌─────────────┐
           │  WSARecv    │  (Async, returns immediately)
           └──────┬──────┘
                  │
                  ▼
           ┌──────────────────┐
           │ GQCS (blocked)   │
           └──────┬───────────┘
                  │ [Data arrives]
                  ▼
           ┌──────────────┐
           │ OnRecv       │
           └──────┬───────┘
                  │
                  ├─> Process data
                  ├─> PostSend(response)
                  └─> PostRecv() (continue receiving)

           ┌─────────────┐
           │  WSASend    │  (Async, returns immediately)
           └──────┬──────┘
                  │
                  ▼
           ┌──────────────────┐
           │ GQCS (blocked)   │
           └──────┬───────────┘
                  │ [Send completes]
                  ▼
           ┌──────────────┐
           │ OnSend       │
           └──────────────┘
                  │
                  └─> Log or cleanup
```

### Pattern 2: Echo Server Implementation

```cpp
class EchoServer : public IServerHandler {
public:
    void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) override {
        // Echo: Send back what we received
        std::string_view echoData{ data.data(), data.size() };
        client->PostSend(echoData);

        // Continue receiving
        client->PostRecv();
    }

    void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) override {
        // Send completed, just log
        _logger->Info("Echoed {} bytes to client #{}", bytesTransferred, client->socket);
    }
};
```

### Pattern 3: Continuous Accept

To handle concurrent connection requests, pre-post multiple accepts:

```cpp
// At server startup
_acceptor->PostAccepts(_config.server.backlog);  // e.g., 10 concurrent accepts

// In OnAcceptComplete
void OnAcceptComplete(...) {
    // Process accepted connection
    // ...

    // Immediately re-post accept (keeps backlog full)
    PostAccept();
}
```

This ensures `backlog` number of accept operations are always pending.

### Pattern 4: Graceful Disconnect Detection

```cpp
void HandleRecv(CompletionEvent& event) {
    // Zero bytes or failure indicates disconnect
    if (!event.success || event.bytesTransferred == 0) {
        _logger->Info("Client disconnected gracefully");
        _handler->OnDisconnect(clientPtr);
        CloseClient(clientPtr, true);
        return;
    }

    // Normal receive processing
    // ...
}
```

### Pattern 5: Fire-and-Forget Operations

Some operations don't require waiting for completion:

```cpp
// Post send and continue (completion will arrive asynchronously)
client->PostSend(data);

// Don't block waiting for send completion
// Worker thread will handle it eventually
```

### Pattern 6: Chaining Operations

After receiving data, chain send and next receive:

```cpp
void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
    // 1. Process received data
    auto response = ProcessData(data);

    // 2. Send response
    client->PostSend(response);

    // 3. Continue receiving (don't wait for send completion)
    client->PostRecv();
}
```

Both send and receive are now pending simultaneously on the client.

### Anti-Patterns to Avoid

#### Anti-Pattern 1: Blocking in Completion Handler

```cpp
// BAD: Blocks worker thread
void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
    std::this_thread::sleep_for(std::chrono::seconds(1));  // DON'T!
    client->PostSend(data);
}
```

Worker threads are limited. Blocking ties up resources.

**Solution**: Offload long operations to separate thread pool.

#### Anti-Pattern 2: Forgetting to Re-Post

```cpp
// BAD: Doesn't re-post receive
void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
    client->PostSend(data);
    // Missing: client->PostRecv();
}
```

Client won't receive more data. Connection appears hung.

#### Anti-Pattern 3: Multiple Overlapped on Same Buffer

```cpp
// BAD: Reuses same overlapped before previous completes
client->PostRecv();
client->PostRecv();  // Second call overwrites first!
```

Each client has one recv overlapped. Calling `PostRecv()` twice before first completes corrupts state.

**Solution**: Use array of overlapped structures if pipelining needed.

---

## Error Handling

### Result Type

The network layer uses `fn::Result<T, E>` for error propagation:

```cpp
template<typename T, typename E>
class Result {
public:
    static Result Ok(T value);
    static Result Err(E error);

    bool HasErr() const;
    bool HasValue() const;

    T& Value();
    E& Error();
};
```

### Error Checking Patterns

#### Pattern 1: Immediate Return

```cpp
Res PostAccept() {
    auto overlapped = _pool.Acquire();
    if (!overlapped) {
        return Res::Err(err::ENetworkError::OutOfMemory);
    }

    // Continue...
}
```

#### Pattern 2: GUARD Macro

```cpp
GUARD(LoadAcceptExFunctions());
// If error, immediately returns

// Continue if successful
PostAccepts(backlog);
```

The `GUARD` macro expands to:
```cpp
if (auto res = LoadAcceptExFunctions(); res.HasErr()) {
    return res;
}
```

#### Pattern 3: GUARD_EFFECT Macro

```cpp
GUARD_EFFECT(PostAccept(), [this]() {
    _logger->Error("Failed to re-post AcceptEx after completion.");
});
```

Executes effect function on error before returning.

### Network Error Types

```cpp
enum class ENetworkError {
    // IOCP errors
    IocpCreateFailed,
    IocpConnectFailed,
    IocpInternalError,

    // Socket errors
    SocketCreateFailed,
    SocketBindFailed,
    SocketListenFailed,
    SocketAcceptFailed,
    SocketPostAcceptFailed,

    // I/O errors
    WsaRecvFailed,
    WsaSendFailed,

    // Threading errors
    ThreadAcceptFailed,

    // Memory errors
    OutOfMemory,
};
```

### Windows Error Codes

Many operations set Windows error codes via `GetLastError()` or `WSAGetLastError()`:

```cpp
int result = WSARecv(...);
if (result == SOCKET_ERROR) {
    DWORD error = WSAGetLastError();
    if (error != ERROR_IO_PENDING) {
        // Actual error
        _logger->Error("WSARecv failed: {}", error);
        return Res::Err(err::ENetworkError::WsaRecvFailed);
    }
    // ERROR_IO_PENDING is expected (operation pending)
}
```

### Common Error Scenarios

#### Scenario 1: ERROR_IO_PENDING

**Not an error**: Indicates async operation successfully queued.

```cpp
if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
    // Only log if NOT pending
    _logger->Error("Operation failed");
}
```

#### Scenario 2: GetQueuedCompletionStatus Failure

```cpp
BOOL ok = GetQueuedCompletionStatus(...);
if (ok == FALSE) {
    DWORD error = GetLastError();
    if (error == ERROR_ABANDONED_WAIT_0) {
        // IOCP handle closed
        break;
    }
    // Other error
    event.success = false;
    event.errorCode = error;
}
```

#### Scenario 3: Client Pool Exhaustion

```cpp
auto client = FindAvailableClient();
if (!client) {
    _logger->Error("Client pool full!");
    closesocket(ctx.acceptSocket);  // Reject connection
    return;
}
```

**Configuration**: Increase `server.max_clients` in runtime config.

#### Scenario 4: Socket Association Failure

```cpp
if (auto res = _iocp->AssociateSocket(socket, client.get()); res.HasErr()) {
    _logger->Error("Failed to associate socket with IOCP.");
    client->Close(true);  // Cleanup
    return;
}
```

### Logging Strategy

**Error Logging**: Always log errors with context:

```cpp
_logger->Error("Failed to create accept socket. error: {}", WSAGetLastError());
```

**Info Logging**: Log significant events:

```cpp
_logger->Info("Client connected. socket: {}, ip: {}", socket, ipAddress);
```

**Debug Logging**: Verbose operation details:

```cpp
_logger->Debug("Posted {} AcceptEx requests.", count);
```

### Resource Cleanup on Error

Always cleanup resources on error paths:

```cpp
Res PostAccept() {
    auto overlapped = AcquireOverlapped();
    SocketHandle acceptSocket = CreateAcceptSocket();

    if (acceptSocket == InvalidSocket) {
        ReleaseOverlapped(overlapped);  // Cleanup!
        return Res::Err(err::ENetworkError::SocketCreateFailed);
    }

    BOOL result = _fnAcceptEx(...);
    if (result == FALSE && WSAGetLastError() != ERROR_IO_PENDING) {
        closesocket(acceptSocket);      // Cleanup!
        ReleaseOverlapped(overlapped);  // Cleanup!
        return Res::Err(err::ENetworkError::SocketPostAcceptFailed);
    }

    return Res::Ok();
}
```

---

## Code Examples

### Example 1: Complete Server Setup

```cpp
#include "ServerLifecycle.h"
#include "IServerHandler.h"
#include "WindowsAsyncSocket.h"
#include "NetworkCfg.h"

class MyServer : public IServerHandler {
public:
    MyServer(std::shared_ptr<log::Logger> logger)
        : _logger(logger)
        , _lifecycle(logger, this) {}

    bool Start() {
        // 1. Load configuration
        auto config = NetworkCfg::FromFile("config.runtime.toml");

        // 2. Create listen socket
        auto listenSocket = std::make_shared<WindowsAsyncSocket>(_logger);
        if (auto res = listenSocket->Bind("127.0.0.1", config.server.port); res.HasErr()) {
            _logger->Error("Failed to bind socket");
            return false;
        }
        if (auto res = listenSocket->Listen(config.server.backlog); res.HasErr()) {
            _logger->Error("Failed to listen");
            return false;
        }

        // 3. Start server lifecycle
        if (auto res = _lifecycle.Start(listenSocket, config); res.HasErr()) {
            _logger->Error("Failed to start server");
            return false;
        }

        _logger->Info("Server started on port {}", config.server.port);
        return true;
    }

    void Stop() {
        _lifecycle.Stop();
        _logger->Info("Server stopped");
    }

    // IServerHandler implementation
    void OnAccept(std::shared_ptr<Client> client) override {
        _logger->Info("Client #{} connected", client->socket);
    }

    void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) override {
        _logger->Info("Received {} bytes from client #{}", data.size(), client->socket);
        // Echo back
        client->PostSend(std::string_view{ data.data(), data.size() });
        // Continue receiving
        client->PostRecv();
    }

    void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) override {
        _logger->Info("Sent {} bytes to client #{}", bytesTransferred, client->socket);
    }

    void OnDisconnect(std::shared_ptr<Client> client) override {
        _logger->Info("Client #{} disconnected", client->socket);
    }

private:
    std::shared_ptr<log::Logger> _logger;
    ServerLifeCycle _lifecycle;
};

// Usage
int main() {
    auto logger = log::Logger::Create("MyServer");
    MyServer server(logger);

    if (!server.Start()) {
        return 1;
    }

    // Keep running
    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();

    server.Stop();
    return 0;
}
```

### Example 2: Custom Protocol Handler

```cpp
class ProtocolServer : public IServerHandler {
public:
    void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) override {
        // Parse protocol header
        if (data.size() < sizeof(ProtocolHeader)) {
            _logger->Warn("Incomplete header from client #{}", client->socket);
            client->PostRecv();  // Wait for more data
            return;
        }

        auto* header = reinterpret_cast<const ProtocolHeader*>(data.data());

        switch (header->type) {
            case MessageType::Login:
                HandleLogin(client, data.subspan(sizeof(ProtocolHeader)));
                break;

            case MessageType::Chat:
                HandleChat(client, data.subspan(sizeof(ProtocolHeader)));
                break;

            case MessageType::Disconnect:
                _lifecycle.CloseClient(client, false);  // Graceful close
                return;

            default:
                _logger->Warn("Unknown message type: {}", static_cast<int>(header->type));
                break;
        }

        // Continue receiving
        client->PostRecv();
    }

private:
    void HandleLogin(std::shared_ptr<Client> client, std::span<const char> payload) {
        // Process login
        // ...

        // Send response
        LoginResponse response{ .success = true };
        client->PostSend(std::string_view{
            reinterpret_cast<char*>(&response),
            sizeof(response)
        });
    }

    void HandleChat(std::shared_ptr<Client> client, std::span<const char> payload) {
        // Broadcast to all clients
        BroadcastMessage(payload);
    }
};
```

### Example 3: Per-Client State Management

```cpp
struct ClientState {
    bool authenticated = false;
    std::string username;
    std::chrono::steady_clock::time_point lastActivity;
};

class StatefulServer : public IServerHandler {
public:
    void OnAccept(std::shared_ptr<Client> client) override {
        // Create per-client state
        std::lock_guard lock(_statesMutex);
        _clientStates[client->socket] = ClientState{
            .lastActivity = std::chrono::steady_clock::now()
        };
    }

    void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) override {
        std::lock_guard lock(_statesMutex);
        auto it = _clientStates.find(client->socket);
        if (it == _clientStates.end()) {
            return;  // Unknown client
        }

        auto& state = it->second;
        state.lastActivity = std::chrono::steady_clock::now();

        if (!state.authenticated) {
            // Require authentication first
            HandleAuthAttempt(client, state, data);
        } else {
            // Process authenticated request
            HandleRequest(client, state, data);
        }

        client->PostRecv();
    }

    void OnDisconnect(std::shared_ptr<Client> client) override {
        std::lock_guard lock(_statesMutex);
        _clientStates.erase(client->socket);
    }

private:
    std::mutex _statesMutex;
    std::unordered_map<SocketHandle, ClientState> _clientStates;
};
```

### Example 4: Rate Limiting

```cpp
class RateLimitedServer : public IServerHandler {
public:
    void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) override {
        if (!CheckRateLimit(client->socket)) {
            _logger->Warn("Rate limit exceeded for client #{}", client->socket);
            // Optionally disconnect
            _lifecycle.CloseClient(client, true);
            return;
        }

        // Process normally
        ProcessData(client, data);
        client->PostRecv();
    }

private:
    bool CheckRateLimit(SocketHandle socket) {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(_rateMutex);

        auto& bucket = _rateBuckets[socket];

        // Token bucket algorithm
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - bucket.lastRefill);

        bucket.tokens += elapsed.count() * bucket.refillRate;
        bucket.tokens = std::min(bucket.tokens, bucket.capacity);
        bucket.lastRefill = now;

        if (bucket.tokens >= 1.0f) {
            bucket.tokens -= 1.0f;
            return true;
        }

        return false;
    }

    struct RateBucket {
        float tokens = 10.0f;
        float capacity = 10.0f;
        float refillRate = 0.1f;  // tokens per millisecond
        std::chrono::steady_clock::time_point lastRefill;
    };

    std::mutex _rateMutex;
    std::unordered_map<SocketHandle, RateBucket> _rateBuckets;
};
```

---

## Performance Considerations

### Worker Thread Count Tuning

**Optimal Formula**:
```
workerCount = CPU_CORES * multiplier
```

**Recommendations**:
- **CPU-bound**: `multiplier = 1.0` (exactly CPU cores)
- **I/O-bound**: `multiplier = 2.0` (default, handles blocking operations)
- **Mixed workload**: `multiplier = 1.5`

**Measurement**: Monitor thread utilization:
```cpp
// Windows Performance Monitor
// Counters: Thread\% Processor Time
// Look for: Idle threads vs. active threads
```

### Buffer Size Tuning

**Current Defaults**:
```cpp
recvBufferSize = 4096;  // 4 KB
sendBufferSize = 1024;  // 1 KB
```

**Considerations**:
- **Larger buffers**: Fewer recv calls, higher memory usage
- **Smaller buffers**: More recv calls, lower memory usage
- **Typical packet size**: Match buffer to expected message size
- **MTU awareness**: Network MTU typically 1500 bytes (Ethernet)

**Tuning Strategy**:
1. Profile typical message sizes in your application
2. Set recv buffer to accommodate 95th percentile message size
3. Set send buffer to match or slightly exceed recv buffer
4. Measure impact on throughput and memory

### Client Pool Sizing

**Trade-offs**:
- **Large pool**: Handles connection spikes, more memory
- **Small pool**: Lower memory, may reject connections under load

**Formula**:
```
maxClients = expectedConcurrentConnections * safetyFactor
```

**Example**:
- Expected concurrent: 500 connections
- Safety factor: 1.5
- Pool size: 750 clients

**Memory Impact**:
```
perClientMemory = sizeof(Client) + recvBufferSize + sendBufferSize
                ≈ 64 bytes + 4096 + 1024 = 5184 bytes ≈ 5 KB

totalMemory = maxClients * perClientMemory
            = 1000 * 5 KB = 5 MB
```

### Overlapped Structure Pooling

**IocpAcceptor** uses `ObjectPool<AcceptOverlapped>` to avoid allocation overhead:

```cpp
// Pre-allocate during initialization
IocpAcceptor acceptor(logger, backlog);

// Reuse overlapped structures
auto* overlapped = _pool.Acquire();  // O(1) if available
// ... use ...
_pool.Release(overlapped);           // O(1)
```

**Benefits**:
- No allocation in hot path
- Better cache locality
- Predictable memory usage

### Zero-Copy Optimizations

**Current Design**:
```cpp
// One copy: Application buffer → sendOverlapped.buf
client->PostSend(applicationData);  // CopyMemory inside
```

**Potential Improvement**: Use scatter-gather I/O with `WSABUF` array:
```cpp
WSABUF buffers[2];
buffers[0] = { headerSize, headerPtr };
buffers[1] = { dataSize, dataPtr };
WSASend(socket, buffers, 2, ...);  // No intermediate copy
```

### Lock Contention

**Current Bottlenecks**:
1. Client state maps (if application uses them)
2. Logger (mitigated by async logging in spdlog)
3. Client pool search (no lock currently, potential race)

**Mitigation Strategies**:
- **Sharding**: Partition client state by socket hash
- **Lock-free structures**: Use atomic operations where possible
- **Reader-writer locks**: If read-heavy workload

### IOCP Completion Batching

**Advanced Technique**: Use `GetQueuedCompletionStatusEx` to dequeue multiple completions at once:

```cpp
OVERLAPPED_ENTRY entries[32];
ULONG numEntries = 0;
GetQueuedCompletionStatusEx(
    _handle,
    entries,
    32,          // Batch size
    &numEntries,
    INFINITE,
    FALSE);

// Process batch
for (ULONG i = 0; i < numEntries; ++i) {
    // Handle entries[i]
}
```

**Benefits**:
- Reduced kernel transitions
- Better cache utilization
- Higher throughput under heavy load

### Memory Locality

**Client Pool Allocation**:
```cpp
// Reserve contiguous memory
_clientPool.reserve(maxClients);
for (int i = 0; i < maxClients; ++i) {
    _clientPool.emplace_back(std::make_shared<Client>());
}
```

**Cache Efficiency**: Sequential iteration over pool is cache-friendly.

**Potential Improvement**: Use flat array instead of `shared_ptr` vector:
```cpp
std::vector<Client> _clientPool;  // Clients directly in vector
```

### Profiling Recommendations

**Tools**:
- **Visual Studio Profiler**: CPU sampling, instrumentation
- **Windows Performance Analyzer**: System-wide tracing
- **ETW (Event Tracing for Windows)**: Kernel-level insights

**Key Metrics**:
- Thread utilization per worker
- IOCP queue depth
- Completion latency (time from I/O initiation to handler invocation)
- Throughput (operations per second)
- Memory usage (working set, commit size)

---

## Best Practices

### DO: Pre-Post Multiple Accepts

```cpp
// Good: Handles concurrent connection attempts
_acceptor->PostAccepts(backlog);  // e.g., 10
```

### DO: Re-Post Operations Immediately

```cpp
void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
    ProcessData(data);
    client->PostSend(response);
    client->PostRecv();  // Don't forget!
}
```

### DO: Use Result Types for Error Handling

```cpp
// Good: Explicit error handling
Res Initialize() {
    if (auto res = LoadFunctions(); res.HasErr()) {
        return res;
    }
    return Res::Ok();
}
```

### DO: Log Errors with Context

```cpp
// Good: Actionable logging
_logger->Error("Failed to accept connection. error: {}, backlog: {}",
    WSAGetLastError(), _backlog);
```

### DO: Use RAII for Resource Management

```cpp
// Good: Automatic cleanup
class IocpIoMultiplexer {
    ~IocpIoMultiplexer() noexcept {
        Shutdown();  // Automatic
    }
};
```

### DON'T: Block in Completion Handlers

```cpp
// Bad: Ties up worker thread
void OnRecv(...) {
    std::this_thread::sleep_for(1s);  // DON'T!
}

// Good: Offload to separate thread
void OnRecv(...) {
    _threadPool.Post([data] {
        ProcessSlowOperation(data);
    });
}
```

### DON'T: Perform Heavy Computation in Handlers

```cpp
// Bad: Blocks other completions
void OnRecv(...) {
    auto result = ExpensiveComputation(data);  // DON'T!
    client->PostSend(result);
}

// Good: Use dedicated worker pool
void OnRecv(...) {
    _computePool.Enqueue([client, data] {
        auto result = ExpensiveComputation(data);
        client->PostSend(result);
    });
}
```

### DON'T: Share Overlapped Structures

```cpp
// Bad: Race condition
RecvOverlapped sharedOverlapped;  // DON'T!
client1->PostRecv(&sharedOverlapped);
client2->PostRecv(&sharedOverlapped);

// Good: Per-client overlapped
struct Client {
    RecvOverlapped recvOverlapped;  // Dedicated
};
```

### DON'T: Forget SO_UPDATE_ACCEPT_CONTEXT

```cpp
// Bad: Accepted socket won't work correctly
AcceptEx(listenSocket, acceptSocket, ...);

// Good: Update context
AcceptEx(listenSocket, acceptSocket, ...);
setsockopt(acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, ...);
```

### Configuration Management

**Separate Compile-Time and Runtime Config**:
- **Compile-time** (`Const.h`): Buffer sizes, limits (regenerate on change)
- **Runtime** (`NetworkCfg.h`): Port, thread counts, backlog (load at startup)

**Use Environment Variables for Overrides**:
```bash
# Override port without changing config file
set SERVER_PORT=9090
echo-server.exe
```

### Testing Strategies

**Unit Testing**: Mock IOCP behavior (difficult, consider integration tests)

**Integration Testing**: Use real echo client to validate:
```powershell
echo-server.exe
echo-client.exe
```

**Load Testing**: Tools like `ApacheBench`, `wrk`, or custom stress client

**Stress Testing**: Simulate connection storms, slow clients, disconnects

---

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - Overall system architecture
- [IOCP_EchoServer_Architecture.md](IOCP_EchoServer_Architecture.md) - Echo server specifics
- [EchoServer_Sequence_diagram.md](EchoServer_Sequence_diagram.md) - Sequence diagrams
- [CLAUDE.md](../CLAUDE.md) - Build and development guide

---

## References

**Windows IOCP Documentation**:
- [I/O Completion Ports (Microsoft Docs)](https://docs.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
- [GetQueuedCompletionStatus](https://docs.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus)
- [AcceptEx](https://docs.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex)

**Winsock Documentation**:
- [WSARecv](https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsarecv)
- [WSASend](https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasend)
- [WSASocket](https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasocketw)

**Performance Resources**:
- [Windows Performance Analyzer](https://docs.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-analyzer)
- [Concurrency Runtime Best Practices](https://docs.microsoft.com/en-us/cpp/parallel/concrt/best-practices-in-the-concurrency-runtime)

---

**Last Updated**: 2026-02-05
**Version**: 1.0
**Maintainer**: Network Layer Team


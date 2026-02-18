# API Reference

Complete API documentation for the highp-mmorpg network layer and utility libraries.

**Version:** 1.0
**Last Updated:** 2026-02-05
**Platform:** Windows (IOCP-based)

## Table of Contents

- [Network Layer](#network-layer)
  - [IocpIoMultiplexer](#iocpiomultiplexer)
  - [IocpAcceptor](#iocpacceptor)
  - [ServerLifeCycle](#serverlifecycle)
  - [Client](#client)
  - [ISocket](#isocket)
  - [IServerHandler](#iserverhandler)
  - [WindowsAsyncSocket](#windowsasyncsocket)
- [Network Structures](#network-structures)
  - [CompletionEvent](#completionevent)
  - [AcceptContext](#acceptcontext)
  - [OverlappedExt](#overlappedext)
  - [EIoType](#eiotype)
- [Utilities](#utilities)
  - [Result&lt;T, E&gt;](#resultt-e)
  - [Logger](#logger)
  - [TextLogger](#textlogger)
  - [ObjectPool&lt;T&gt;](#objectpoolt)
  - [ENetworkError](#enetworkerror)
- [Configuration](#configuration)
  - [NetworkCfg](#networkcfg)
  - [Const](#const)
- [Platform Types](#platform-types)
- [Usage Examples](#usage-examples)

---

## Network Layer

### IocpIoMultiplexer

Windows I/O Completion Port (IOCP) based I/O multiplexer. Manages IOCP kernel object, worker thread pool, and socket associations.

**Namespace:** `highp::network`
**Header:** `network/IocpIoMultiplexer.h`

#### Constructor

```cpp
explicit IocpIoMultiplexer(
    std::shared_ptr<log::Logger> logger,
    CompletionHandler handler = nullptr
);
```

**Parameters:**

- `logger` - Logger instance for diagnostic output
- `handler` - Optional completion event callback (can be set later via `SetCompletionHandler`)

**Thread Safety:** Constructor is not thread-safe. Should be called from a single thread.

**Example:**

```cpp
auto logger = log::Logger::Default<log::TextLogger>();
auto multiplexer = std::make_unique<IocpIoMultiplexer>(logger);
```

#### Initialize

```cpp
Res Initialize(int workerThreadCount);
```

Creates IOCP kernel object and spawns worker thread pool.

**Parameters:**

- `workerThreadCount` - Number of worker threads to create (typically `std::thread::hardware_concurrency()`)

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Initialization successful
- `Err(ENetworkError::IocpCreateFailed)` - IOCP creation failed
- `Err(ENetworkError::ThreadWorkerCreateFailed)` - Thread creation failed

**Thread Safety:** Not thread-safe. Call once before any socket operations.

**Side Effects:**

- Creates IOCP handle via `CreateIoCompletionPort()`
- Spawns `workerThreadCount` std::jthread instances
- Sets internal running state to true

**Preconditions:**

- Must not be already initialized
- `workerThreadCount` must be > 0

**Example:**

```cpp
auto result = multiplexer->Initialize(std::thread::hardware_concurrency());
if (result.HasErr()) {
    logger->Error("IOCP initialization failed: {}", ToString(result.Err()));
    return;
}
```

#### Shutdown

```cpp
void Shutdown();
```

Stops all worker threads and releases IOCP resources.

**Thread Safety:** Thread-safe. Can be called from any thread.

**Side Effects:**

- Posts termination messages to IOCP queue (one per worker thread)
- Waits for all worker threads to complete via std::jthread destructor
- Closes IOCP handle
- Sets running state to false

**Postconditions:**

- All worker threads have terminated
- IOCP handle is invalid
- No further completion events will be processed

**Example:**

```cpp
multiplexer->Shutdown();
```

#### AssociateSocket

```cpp
Res AssociateSocket(SocketHandle socket, void* completionKey);
```

Associates a socket with the IOCP for asynchronous I/O completion notifications.

**Parameters:**

- `socket` - Socket handle to associate
- `completionKey` - User-defined key returned in completion events (typically `Client*`)

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Association successful
- `Err(ENetworkError::IocpConnectFailed)` - Association failed

**Thread Safety:** Thread-safe after initialization.

**Preconditions:**

- IOCP must be initialized
- Socket must be valid
- Socket should not be already associated with another IOCP

**Example:**

```cpp
auto client = std::make_shared<Client>();
client->socket = acceptedSocket;
auto result = multiplexer->AssociateSocket(client->socket, client.get());
GUARD(result);
```

#### PostCompletion

```cpp
Res PostCompletion(DWORD bytes, void* key, LPOVERLAPPED overlapped);
```

Manually posts a completion event to the IOCP queue.

**Parameters:**

- `bytes` - Number of bytes transferred (arbitrary for manual posts)
- `key` - Completion key
- `overlapped` - OVERLAPPED structure pointer (can be nullptr)

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Post successful
- `Err(ENetworkError::IocpInternalError)` - Post failed

**Thread Safety:** Thread-safe.

**Usage:** Primarily used for signaling worker threads to terminate during shutdown.

**Example:**

```cpp
// Signal worker thread termination
multiplexer->PostCompletion(0, nullptr, nullptr);
```

#### SetCompletionHandler

```cpp
void SetCompletionHandler(CompletionHandler handler);
```

Registers the callback function invoked when completion events occur.

**Parameters:**

- `handler` - Function matching signature `void(CompletionEvent)`

**Thread Safety:** Not thread-safe. Should be set before calling `Initialize()`.

**Example:**

```cpp
multiplexer->SetCompletionHandler([this](CompletionEvent event) {
    switch (event.ioType) {
        case EIoType::Accept: HandleAccept(event); break;
        case EIoType::Recv:   HandleRecv(event); break;
        case EIoType::Send:   HandleSend(event); break;
    }
});
```

#### GetHandle

```cpp
HANDLE GetHandle() const noexcept;
```

Returns the underlying IOCP kernel handle.

**Returns:** IOCP handle (INVALID_HANDLE_VALUE if not initialized)

**Thread Safety:** Thread-safe.

**Usage:** Required for passing to `IocpAcceptor::Initialize()`.

**Example:**

```cpp
HANDLE iocpHandle = multiplexer->GetHandle();
acceptor->Initialize(listenSocket, iocpHandle);
```

#### IsRunning

```cpp
bool IsRunning() const noexcept;
```

Checks if the IOCP is currently running.

**Returns:** `true` if initialized and not shut down, `false` otherwise

**Thread Safety:** Thread-safe (atomic read).

---

### IocpAcceptor

AcceptEx-based asynchronous connection acceptor. Manages accept socket pool and completion handling.

**Namespace:** `highp::network`
**Header:** `network/IocpAcceptor.h`

#### Constructor

```cpp
explicit IocpAcceptor(
    std::shared_ptr<log::Logger> logger,
    std::shared_ptr<SocketOptionBuilder> socketOptionBuilder = nullptr,
    int preAllocCount = 10,
    AcceptCallback onAfterAccept = nullptr
);
```

**Parameters:**

- `logger` - Logger instance
- `socketOptionBuilder` - Optional socket option configurator
- `preAllocCount` - Number of AcceptOverlapped structures to pre-allocate (default: 10)
- `onAfterAccept` - Optional accept completion callback

**Example:**

```cpp
auto acceptor = std::make_unique<IocpAcceptor>(logger, nullptr, 10);
```

#### Initialize

```cpp
Res Initialize(SocketHandle listenSocket, HANDLE iocpHandle);
```

Initializes the acceptor by associating the listen socket with IOCP and loading AcceptEx function pointers.

**Parameters:**

- `listenSocket` - Socket in listen state
- `iocpHandle` - IOCP handle from `IocpIoMultiplexer::GetHandle()`

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Initialization successful
- `Err(ENetworkError::IocpConnectFailed)` - Failed to associate socket with IOCP
- `Err(ENetworkError::SocketInvalid)` - Invalid listen socket
- Various WSA errors if AcceptEx function loading fails

**Thread Safety:** Not thread-safe. Call once during server startup.

**Side Effects:**

- Loads AcceptEx and GetAcceptExSockAddrs function pointers via `WSAIoctl`
- Associates listen socket with IOCP

**Example:**

```cpp
auto result = acceptor->Initialize(listenSocket->GetSocketHandle(),
                                    multiplexer->GetHandle());
GUARD(result);
```

#### PostAccept

```cpp
Res PostAccept();
```

Posts a single asynchronous AcceptEx request.

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Accept request posted successfully
- `Err(ENetworkError::SocketPostAcceptFailed)` - AcceptEx call failed

**Thread Safety:** Thread-safe after initialization.

**Side Effects:**

- Creates new accept socket
- Acquires AcceptOverlapped from pool (or allocates new)
- Calls AcceptEx with overlapped I/O

**Behavior:**

- If AcceptEx completes synchronously (WSA_IO_PENDING not set), returns immediately
- If AcceptEx completes asynchronously, completion notification arrives via IOCP

**Example:**

```cpp
GUARD(acceptor->PostAccept());
```

#### PostAccepts

```cpp
Res PostAccepts(int count);
```

Posts multiple asynchronous AcceptEx requests.

**Parameters:**

- `count` - Number of accept requests to post (typically equal to backlog size)

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - All requests posted successfully
- `Err(...)` - First error encountered

**Thread Safety:** Thread-safe after initialization.

**Usage:** Called during server startup to prepare for concurrent incoming connections.

**Example:**

```cpp
// Post backlog number of accepts
GUARD(acceptor->PostAccepts(config.server.backlog));
```

#### OnAcceptComplete

```cpp
Res OnAcceptComplete(AcceptOverlapped* overlapped, DWORD bytesTransferred);
```

Handles AcceptEx completion. Called by IOCP worker threads via completion handler.

**Parameters:**

- `overlapped` - Completed AcceptOverlapped structure
- `bytesTransferred` - Bytes transferred (typically 0 for AcceptEx)

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Accept handled successfully
- `Err(...)` - Error during accept processing

**Thread Safety:** Thread-safe. Called from multiple worker threads.

**Side Effects:**

- Sets SO_UPDATE_ACCEPT_CONTEXT on accepted socket
- Parses local/remote addresses via GetAcceptExSockAddrs
- Invokes registered AcceptCallback
- Automatically posts new AcceptEx request to maintain accept pool
- Returns AcceptOverlapped to pool

**Preconditions:**

- Overlapped must be from this acceptor
- AcceptCallback must be registered

**Example:**

```cpp
// Called from IOCP worker thread
if (event.ioType == EIoType::Accept) {
    auto* acceptOverlapped = reinterpret_cast<AcceptOverlapped*>(event.overlapped);
    auto result = acceptor->OnAcceptComplete(acceptOverlapped, event.bytesTransferred);
    if (result.HasErr()) {
        logger->Error("Accept completion failed");
    }
}
```

#### SetAcceptCallback

```cpp
void SetAcceptCallback(AcceptCallback callback);
```

Registers callback invoked when new connections are accepted.

**Parameters:**

- `callback` - Function matching signature `void(AcceptContext&)`

**Thread Safety:** Not thread-safe. Should be set before accepting connections.

**Example:**

```cpp
acceptor->SetAcceptCallback([this](AcceptContext& ctx) {
    auto client = FindAvailableClient();
    client->socket = ctx.acceptSocket;
    multiplexer->AssociateSocket(client->socket, client.get());
    client->PostRecv();
    handler->OnAccept(client);
});
```

#### Shutdown

```cpp
void Shutdown();
```

Stops accepting new connections and releases resources.

**Thread Safety:** Thread-safe.

**Side Effects:**

- Clears AcceptOverlapped pool
- Resets function pointers
- Invalidates listen socket reference

---

### ServerLifeCycle

High-level server lifecycle manager. Orchestrates IOCP, Acceptor, and client management.

**Namespace:** `highp::network`
**Header:** `network/ServerLifecycle.h`

#### Constructor

```cpp
explicit ServerLifeCycle(
    std::shared_ptr<log::Logger> logger,
    IServerHandler* handler
);
```

**Parameters:**

- `logger` - Logger instance
- `handler` - Application-layer event handler implementing `IServerHandler`

**Example:**

```cpp
class MyServer : public IServerHandler { /* ... */ };

MyServer myHandler;
auto lifecycle = std::make_unique<ServerLifeCycle>(logger, &myHandler);
```

#### Start

```cpp
Res Start(std::shared_ptr<ISocket> listenSocket, const NetworkCfg& config);
```

Starts the server with specified configuration.

**Parameters:**

- `listenSocket` - Initialized and listening socket
- `config` - Runtime network configuration

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Server started successfully
- `Err(...)` - Initialization error

**Thread Safety:** Not thread-safe. Call once during startup.

**Side Effects:**

- Initializes IOCP with configured thread count
- Initializes acceptor with listen socket
- Pre-allocates client pool
- Posts initial AcceptEx requests
- Starts processing I/O completions

**Preconditions:**

- Listen socket must be in listen state
- Handler must be valid

**Example:**

```cpp
auto listenSocket = std::make_shared<WindowsAsyncSocket>(logger);
GUARD(listenSocket->Initialize());
GUARD(listenSocket->CreateSocket(NetworkTransport(ETransport::TCP)));
GUARD(listenSocket->Bind(config.server.port));
GUARD(listenSocket->Listen(config.server.backlog));

auto result = lifecycle->Start(listenSocket, config);
if (result.HasErr()) {
    logger->Error("Server start failed: {}", ToString(result.Err()));
}
```

#### Stop

```cpp
void Stop();
```

Gracefully stops the server and releases all resources.

**Thread Safety:** Thread-safe. Can be called from any thread.

**Side Effects:**

- Shuts down acceptor (stops accepting new connections)
- Shuts down IOCP (stops worker threads)
- Closes all client connections
- Clears client pool

**Behavior:** Blocks until all worker threads have terminated.

**Example:**

```cpp
lifecycle->Stop();
```

#### CloseClient

```cpp
void CloseClient(std::shared_ptr<Client> client, bool force = true);
```

Closes a client connection.

**Parameters:**

- `client` - Client to disconnect
- `force` - If `true`, closes immediately (linger = 0); if `false`, graceful shutdown

**Thread Safety:** Thread-safe.

**Side Effects:**

- Calls `client->Close(force)`
- Decrements connected client count
- Invokes `handler->OnDisconnect(client)`

**Example:**

```cpp
lifecycle->CloseClient(client, true);
```

#### GetConnectedClientCount

```cpp
size_t GetConnectedClientCount() const noexcept;
```

Returns current number of connected clients.

**Thread Safety:** Thread-safe (atomic read).

**Example:**

```cpp
logger->Info("Active connections: {}", lifecycle->GetConnectedClientCount());
```

#### IsRunning

```cpp
bool IsRunning() const noexcept;
```

Checks if the server is running.

**Returns:** `true` if IOCP is running, `false` otherwise

**Thread Safety:** Thread-safe.

---

### Client

Represents a single client connection with I/O buffers and state.

**Namespace:** `highp::network`
**Header:** `network/Client.h`

#### Constructor

```cpp
Client();
```

Default constructor. Initializes socket to `INVALID_SOCKET`.

**Example:**

```cpp
auto client = std::make_shared<Client>();
```

#### PostRecv

```cpp
Res PostRecv();
```

Posts an asynchronous receive operation.

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Receive posted successfully
- `Err(ENetworkError::WsaRecvFailed)` - WSARecv call failed

**Thread Safety:** Thread-safe for the same client (recv and send are independent).

**Side Effects:**

- Calls `WSARecv` with `recvOverlapped`
- If completes synchronously or asynchronously, completion arrives via IOCP

**Preconditions:**

- Socket must be valid and associated with IOCP
- Previous recv must have completed

**Postconditions:**

- `recvOverlapped` is in use until completion

**Example:**

```cpp
auto result = client->PostRecv();
if (result.HasErr()) {
    logger->Error("PostRecv failed for client");
    CloseClient(client);
}
```

#### PostSend

```cpp
Res PostSend(std::string_view data);
```

Posts an asynchronous send operation.

**Parameters:**

- `data` - Data to send (copied to send buffer, max 1024 bytes)

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Send posted successfully
- `Err(ENetworkError::WsaSendFailed)` - WSASend call failed

**Thread Safety:** Thread-safe for the same client.

**Side Effects:**

- Copies data to `sendOverlapped.buf`
- Calls `WSASend` with `sendOverlapped`

**Preconditions:**

- Socket must be valid and associated with IOCP
- Data size must not exceed `Const::Buffer::sendBufferSize` (1024 bytes)
- Previous send must have completed

**Example:**

```cpp
std::string response = "Hello, client!";
auto result = client->PostSend(response);
GUARD(result);
```

#### Close

```cpp
void Close(bool isFireAndForget);
```

Closes the client socket.

**Parameters:**

- `isFireAndForget` - If `true`, sets linger to 0 (RST); if `false`, graceful FIN

**Thread Safety:** Thread-safe (uses atomic close pattern).

**Side Effects:**

- Sets SO_LINGER if `isFireAndForget == true`
- Calls `closesocket()`
- Sets socket to `INVALID_SOCKET`

**Example:**

```cpp
client->Close(true);  // Immediate close with RST
```

#### Members

```cpp
SocketHandle socket;          // Client socket handle
RecvOverlapped recvOverlapped; // Receive operation context
SendOverlapped sendOverlapped; // Send operation context
```

**Note:** Client inherits from `std::enable_shared_from_this<Client>` to safely obtain shared_ptr from `this` in callbacks.

---

### ISocket

Abstract socket interface for platform-independent socket operations.

**Namespace:** `highp::network`
**Header:** `network/ISocket.h`

```cpp
class ISocket {
public:
    using Res = Result<void, ENetworkError>;

    virtual ~ISocket() = default;
    virtual Res Initialize() = 0;
    virtual Res CreateSocket(NetworkTransport transport) = 0;
    virtual Res Bind(unsigned short port) = 0;
    virtual Res Listen(int backlog) = 0;
    virtual Res Cleanup() = 0;
    virtual SocketHandle GetSocketHandle() const = 0;
};
```

**See:** [WindowsAsyncSocket](#windowsasyncsocket) for concrete implementation.

---

### IServerHandler

Application-layer event handler interface. Implement to define server behavior.

**Namespace:** `highp::network`
**Header:** `network/IServerHandler.h`

```cpp
struct IServerHandler {
    virtual ~IServerHandler() = default;

    virtual void OnAccept(std::shared_ptr<Client> client) = 0;
    virtual void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) = 0;
    virtual void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) = 0;
    virtual void OnDisconnect(std::shared_ptr<Client> client) = 0;
};
```

#### OnAccept

Called when a new client connection is accepted.

**Parameters:**

- `client` - Newly connected client

**Thread Safety:** Called from IOCP worker threads. Must be thread-safe.

**Usage:** Perform connection setup, authentication handshakes, etc.

**Example:**

```cpp
void OnAccept(std::shared_ptr<Client> client) override {
    _logger->Info("Client connected");
    // Send welcome message, etc.
}
```

#### OnRecv

Called when data is received from a client.

**Parameters:**

- `client` - Client that sent data
- `data` - Received data span (valid only during callback)

**Thread Safety:** Called from IOCP worker threads. Must be thread-safe.

**Usage:** Parse and process incoming messages.

**Example:**

```cpp
void OnRecv(std::shared_ptr<Client> client, std::span<const char> data) override {
    std::string message(data.data(), data.size());
    _logger->Info("Received: {}", message);
    client->PostSend(message);  // Echo back
}
```

#### OnSend

Called when send operation completes.

**Parameters:**

- `client` - Client whose send completed
- `bytesTransferred` - Number of bytes actually sent

**Thread Safety:** Called from IOCP worker threads. Must be thread-safe.

**Usage:** Track throughput, handle partial sends, etc.

**Example:**

```cpp
void OnSend(std::shared_ptr<Client> client, size_t bytesTransferred) override {
    _totalBytesSent += bytesTransferred;
}
```

#### OnDisconnect

Called when a client disconnects.

**Parameters:**

- `client` - Disconnected client

**Thread Safety:** Called from IOCP worker threads. Must be thread-safe.

**Usage:** Cleanup per-client state, log disconnection, etc.

**Example:**

```cpp
void OnDisconnect(std::shared_ptr<Client> client) override {
    _logger->Info("Client disconnected");
    _clientStates.erase(client.get());
}
```

---

### WindowsAsyncSocket

Windows Winsock2 implementation of ISocket.

**Namespace:** `highp::network`
**Header:** `network/WindowsAsyncSocket.h`

#### Constructor

```cpp
explicit WindowsAsyncSocket(std::shared_ptr<log::Logger> logger);
```

**Example:**

```cpp
auto socket = std::make_shared<WindowsAsyncSocket>(logger);
```

#### Initialize

```cpp
virtual Res Initialize() override;
```

Initializes Winsock (calls `WSAStartup`).

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Winsock initialized
- `Err(ENetworkError::WsaStartupFailed)` - WSAStartup failed

**Thread Safety:** Not thread-safe. Call once per process.

**Example:**

```cpp
GUARD(socket->Initialize());
```

#### CreateSocket

```cpp
virtual Res CreateSocket(NetworkTransport transport) override;
```

Creates a socket with specified transport protocol.

**Parameters:**

- `transport` - `NetworkTransport(ETransport::TCP)` or `NetworkTransport(ETransport::UDP)`

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Socket created
- `Err(ENetworkError::SocketCreateFailed)` - Creation failed

**Thread Safety:** Thread-safe after initialization.

**Example:**

```cpp
GUARD(socket->CreateSocket(NetworkTransport(ETransport::TCP)));
```

#### Bind

```cpp
virtual Res Bind(unsigned short port) override;
```

Binds socket to specified port (INADDR_ANY).

**Parameters:**

- `port` - Port number (host byte order)

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Bind successful
- `Err(ENetworkError::SocketBindFailed)` - Bind failed

**Example:**

```cpp
GUARD(socket->Bind(8080));
```

#### Listen

```cpp
virtual Res Listen(int backlog) override;
```

Puts socket into listen state.

**Parameters:**

- `backlog` - Maximum length of pending connection queue

**Returns:** `Result<void, ENetworkError>`

- `Ok()` - Listen successful
- `Err(ENetworkError::SocketListenFailed)` - Listen failed

**Example:**

```cpp
GUARD(socket->Listen(128));
```

#### Cleanup

```cpp
virtual Res Cleanup() override;
```

Closes socket and cleans up Winsock.

**Returns:** `Result<void, ENetworkError>`

**Side Effects:**

- Calls `closesocket()`
- Calls `WSACleanup()`

**Example:**

```cpp
socket->Cleanup();
```

#### GetSocketHandle

```cpp
virtual SocketHandle GetSocketHandle() const override;
```

Returns the underlying socket handle.

**Returns:** `SocketHandle` (SOCKET on Windows)

---

## Network Structures

### CompletionEvent

Represents an IOCP completion event passed to `CompletionHandler`.

**Namespace:** `highp::network`
**Header:** `network/IocpIoMultiplexer.h`

```cpp
struct CompletionEvent {
    EIoType ioType;              // Type of I/O operation
    void* completionKey;         // User-defined key (typically Client*)
    LPOVERLAPPED overlapped;     // OVERLAPPED structure (cast to OverlappedExt*)
    DWORD bytesTransferred;      // Number of bytes transferred
    bool success;                // true if operation succeeded
    DWORD errorCode;             // Error code if success == false (GetLastError value)
};
```

**Usage:**

```cpp
void OnCompletion(CompletionEvent event) {
    if (!event.success) {
        logger->Error("I/O failed with error: {}", event.errorCode);
        return;
    }

    auto* client = static_cast<Client*>(event.completionKey);
    switch (event.ioType) {
        case EIoType::Recv: {
            auto* recvOv = reinterpret_cast<RecvOverlapped*>(event.overlapped);
            std::span<const char> data(recvOv->buf, event.bytesTransferred);
            handler->OnRecv(client->shared_from_this(), data);
            break;
        }
        // ... other cases
    }
}
```

---

### AcceptContext

Connection acceptance context passed to `AcceptCallback`.

**Namespace:** `highp::network`
**Header:** `network/AcceptContext.h`

```cpp
struct AcceptContext {
    SocketHandle acceptSocket;   // Accepted client socket
    SocketHandle listenSocket;   // Listen socket (for SO_UPDATE_ACCEPT_CONTEXT)
    SOCKADDR_IN localAddr;       // Server local address
    SOCKADDR_IN remoteAddr;      // Client remote address
};
```

**Usage:**

```cpp
acceptor->SetAcceptCallback([this](AcceptContext& ctx) {
    char clientIp[32];
    inet_ntop(AF_INET, &ctx.remoteAddr.sin_addr, clientIp, sizeof(clientIp));
    logger->Info("Accepted connection from {}:{}",
                 clientIp, ntohs(ctx.remoteAddr.sin_port));

    auto client = FindAvailableClient();
    client->socket = ctx.acceptSocket;
    // ... continue setup
});
```

---

### OverlappedExt

Extended OVERLAPPED structures for different I/O operations.

**Namespace:** `highp::network`
**Header:** `network/OverlappedExt.h`

#### SendOverlapped

```cpp
struct SendOverlapped {
    WSAOVERLAPPED overlapped;        // MUST be first member
    EIoType ioType = EIoType::Send;
    SocketHandle clientSocket = InvalidSocket;
    WSABUF bufDesc;
    char buf[Const::Buffer::sendBufferSize];  // 1024 bytes
};
```

#### RecvOverlapped

```cpp
struct RecvOverlapped {
    WSAOVERLAPPED overlapped;        // MUST be first member
    EIoType ioType = EIoType::Recv;
    SocketHandle clientSocket = InvalidSocket;
    WSABUF bufDesc;
    char buf[Const::Buffer::recvBufferSize];  // 4096 bytes
};
```

#### AcceptOverlapped

```cpp
struct AcceptOverlapped {
    WSAOVERLAPPED overlapped;        // MUST be first member
    EIoType ioType = EIoType::Accept;
    SocketHandle clientSocket = InvalidSocket;
    WSABUF bufDesc;
    char buf[Const::Buffer::addressBufferSize];  // 64 bytes
};
```

**Critical Invariant:** `WSAOVERLAPPED overlapped` MUST be the first member for safe casting from `LPOVERLAPPED` to `OverlappedExt*`.

**Casting Pattern:**

```cpp
// From IOCP completion
LPOVERLAPPED lpOverlapped = /* from GetQueuedCompletionStatus */;
auto* recvOv = reinterpret_cast<RecvOverlapped*>(lpOverlapped);  // SAFE
```

---

### EIoType

Enumeration of asynchronous I/O operation types.

**Namespace:** `highp::network`
**Header:** `network/EIoType.h`

```cpp
enum class EIoType {
    Accept,      // AcceptEx operation
    Recv,        // WSARecv operation
    Send,        // WSASend operation
    Disconnect   // Disconnect operation
};
```

**Usage:** Set in `OverlappedExt.ioType` to identify operation type in completion handler.

---

## Utilities

### Result&lt;T, E&gt;

Error handling type similar to C++23 `std::expected`.

**Namespace:** `highp::fn`
**Header:** `lib/Result.hpp`

```cpp
template <typename TData, typename E>
class [[nodiscard]] Result final;
```

#### Creation

```cpp
// Success with data
auto result = Result<int, ENetworkError>::Ok(42);

// Success without data
auto result = Result<void, ENetworkError>::Ok();

// Error
auto result = Result<int, ENetworkError>::Err(ENetworkError::SocketInvalid);
```

#### Inspection

```cpp
bool IsOk() const;     // Returns true if success
bool HasErr() const;   // Returns true if error
TData Data() const;    // Returns data (only for Result<T, E>)
E Err() const;         // Returns error code
explicit operator bool() const;  // Returns IsOk()
```

#### Usage Pattern

```cpp
Result<void, ENetworkError> Initialize() {
    if (socket == INVALID_SOCKET)
        return Result<void, ENetworkError>::Err(ENetworkError::SocketInvalid);

    if (WSAStartup(...) != 0)
        return Result<void, ENetworkError>::Err(ENetworkError::WsaStartupFailed);

    return Result<void, ENetworkError>::Ok();
}

// Calling site
auto result = Initialize();
if (result.HasErr()) {
    logger->Error("Initialization failed: {}", ToString(result.Err()));
    return;
}
```

#### Helper Macros

```cpp
// Early return on error
GUARD(expression)         // Returns result on error
GUARD_VOID(expression)    // Returns void on error
GUARD_BREAK(expression)   // Breaks loop on error

// Early return with side effect
GUARD_EFFECT(expression, function)      // Calls function(), then returns result
GUARD_EFFECT_VOID(expression, function) // Calls function(), then returns void
GUARD_EFFECT_BREAK(expression, function) // Calls function(), then breaks
```

**Example:**

```cpp
Result<void, ENetworkError> StartServer() {
    GUARD(socket->Initialize());
    GUARD(socket->CreateSocket(NetworkTransport(ETransport::TCP)));
    GUARD(socket->Bind(8080));
    GUARD(socket->Listen(128));
    return Result<void, ENetworkError>::Ok();
}
```

---

### Logger

Logging facade supporting multiple backends and std::format.

**Namespace:** `highp::log`
**Header:** `lib/Logger.hpp`

#### Constructor

```cpp
explicit Logger(std::unique_ptr<ILogger> impl);
```

#### Factory Methods

```cpp
// Create with default implementation
template <typename Impl>
static std::shared_ptr<Logger> Default();

// Create with constructor arguments
template <typename Impl, typename... Args>
static std::shared_ptr<Logger> DefaultWithArgs(Args&&... args);
```

**Example:**

```cpp
auto logger = log::Logger::Default<log::TextLogger>();
```

#### Logging Methods

```cpp
// Simple string versions
void Info(std::string_view msg);
void Debug(std::string_view msg);
void Warn(std::string_view msg);
void Error(std::string_view msg);
void Exception(std::string_view msg, std::exception const& ex);

// std::format versions
template<class... Args>
void Info(std::format_string<Args...> fmt, Args&&... args);

template<class... Args>
void Debug(std::format_string<Args...> fmt, Args&&... args);

template<class... Args>
void Warn(std::format_string<Args...> fmt, Args&&... args);

template<class... Args>
void Error(std::format_string<Args...> fmt, Args&&... args);
```

**Thread Safety:** Thread-safe. All methods can be called from multiple threads.

**Examples:**

```cpp
logger->Info("Server started on port 8080");
logger->Debug("Accepted connection from {}", clientIp);
logger->Warn("Client pool exhausted. Max: {}", maxClients);
logger->Error("Socket error: {}", WSAGetLastError());

try {
    // ...
} catch (const std::exception& ex) {
    logger->Exception("Unhandled exception", ex);
}
```

---

### TextLogger

Console-based logger implementation.

**Namespace:** `highp::log`
**Header:** `lib/TextLogger.h`

```cpp
class TextLogger : public ILogger {
public:
    TextLogger() = default;
    ~TextLogger() override = default;

    void Info(std::string_view msg) override;
    void Debug(std::string_view msg) override;
    void Warn(std::string_view msg) override;
    void Error(std::string_view msg) override;
    void Exception(std::string_view msg, std::exception const& ex) override;
};
```

**Output Format:** `[LEVEL] message` to stdout/stderr.

**Example:**

```cpp
auto logger = std::make_unique<TextLogger>();
logger->Info("Server initializing...");
// Output: [INFO] Server initializing...
```

---

### ObjectPool&lt;T&gt;

Thread-safe object pool for reducing allocation overhead.

**Namespace:** `highp::mem`
**Header:** `lib/ObjectPool.hpp`

```cpp
template <typename T>
class ObjectPool final;
```

#### Constructor

```cpp
explicit ObjectPool(int preAllocCount = 0);
```

**Parameters:**

- `preAllocCount` - Number of objects to pre-allocate

**Example:**

```cpp
mem::ObjectPool<AcceptOverlapped> pool(10);
```

#### PreAllocate

```cpp
void PreAllocate(int count);
```

Pre-allocates objects to the pool.

**Thread Safety:** Thread-safe.

**Example:**

```cpp
pool.PreAllocate(100);
```

#### Acquire

```cpp
T* Acquire();
```

Acquires an object from the pool. Creates new object if pool is empty.

**Returns:** Raw pointer to object

**Thread Safety:** Thread-safe.

**Ownership:** Pool retains ownership. Caller must return via `Release()`.

**Example:**

```cpp
auto* overlapped = pool.Acquire();
// Use overlapped...
pool.Release(overlapped);
```

#### Release

```cpp
void Release(T* obj);
```

Returns object to the pool for reuse.

**Parameters:**

- `obj` - Object to return (must have been acquired from this pool)

**Thread Safety:** Thread-safe.

**Example:**

```cpp
pool.Release(overlapped);
```

#### Clear

```cpp
void Clear();
```

Releases all objects. All outstanding objects become invalid.

**Thread Safety:** Thread-safe.

**Warning:** Do not call while objects are in use.

#### AvailableCount / TotalCount

```cpp
size_t AvailableCount();  // Number of available objects
size_t TotalCount();      // Total objects in pool
```

**Thread Safety:** Thread-safe.

---

### ENetworkError

Network error enumeration.

**Namespace:** `highp::err`
**Header:** `lib/NetworkError.h`

```cpp
enum class ENetworkError {
    UnknownError,

    // WSA errors
    WsaStartupFailed,
    WsaNotInitialized,
    WsaNetworkSubSystemFailed,
    WsaNotSupportedAddressFamily,
    WsaInvalidArgs,
    WsaLackFileDescriptor,
    WsaNoBufferSpace,
    WsaNotSupportedProtocol,
    WsaRecvFailed,
    WsaSendFailed,

    // Socket errors
    SocketInvalid,
    SocketCreateFailed,
    SocketBindFailed,
    SocketListenFailed,
    SocketAcceptFailed,
    SocketPostAcceptFailed,

    // IOCP errors
    IocpCreateFailed,
    IocpConnectFailed,
    IocpInternalError,
    IocpIoFailed,

    // Thread errors
    ThreadWorkerCreateFailed,
    ThreadAccepterCreateFailed,
    ThreadWorkerFailed,
    ThreadAcceptFailed,
};
```

#### ToString

```cpp
constexpr std::string_view ToString(ENetworkError e);
```

Converts error code to human-readable string.

**Example:**

```cpp
auto result = socket->Initialize();
if (result.HasErr()) {
    logger->Error("Socket initialization failed: {}", ToString(result.Err()));
}
```

---

## Configuration

### NetworkCfg

Runtime network configuration loaded from TOML.

**Namespace:** `highp::network`
**Header:** `network/NetworkCfg.h`

```cpp
struct NetworkCfg {
    struct Server {
        INT port;           // Server port (default: 8080)
        INT maxClients;     // Max concurrent clients (default: 1000)
        INT backlog;        // Listen backlog (default: 10)

        struct Defaults { /* ... */ };
    } server;

    struct Thread {
        INT maxWorkerThreadMultiplier;    // Worker thread multiplier (default: 2)
        INT maxAcceptorThreadCount;       // Acceptor threads (default: 2)
        INT desirableIocpThreadCount;     // IOCP thread count (-1 = auto, default)

        struct Defaults { /* ... */ };
    } thread;

    static NetworkCfg FromFile(const std::filesystem::path& path);
    static NetworkCfg FromCfg(const highp::config::Config& cfg);
    static NetworkCfg WithDefaults();
};
```

#### FromFile

Loads configuration from TOML file with environment variable overrides.

**Parameters:**

- `path` - Path to runtime TOML config file

**Returns:** `NetworkCfg` struct

**Throws:** `std::runtime_error` if file cannot be loaded

**Environment Variables:**

- `SERVER_PORT` - Overrides `server.port`
- `SERVER_MAX_CLIENTS` - Overrides `server.max_clients`
- `SERVER_BACKLOG` - Overrides `server.backlog`
- `THREAD_MAX_WORKER_THREAD_MULTIPLIER` - Overrides `thread.max_worker_thread_multiplier`
- `THREAD_MAX_ACCEPTOR_THREAD_COUNT` - Overrides `thread.max_acceptor_thread_count`
- `THREAD_DESIRABLE_IOCP_THREAD_COUNT` - Overrides `thread.desirable_iocp_thread_count`

**Example:**

```cpp
auto config = NetworkCfg::FromFile("config.runtime.toml");
logger->Info("Server will listen on port {}", config.server.port);
```

#### WithDefaults

Creates configuration with default values.

**Example:**

```cpp
auto config = NetworkCfg::WithDefaults();
config.server.port = 9090;  // Override specific values
```

---

### Const

Compile-time constants loaded from TOML.

**Namespace:** `highp::network`
**Header:** `network/Const.h`

```cpp
struct Const {
    struct Buffer {
        static constexpr INT recvBufferSize = 4096;       // Receive buffer size
        static constexpr INT sendBufferSize = 1024;       // Send buffer size
        static constexpr INT addressBufferSize = 64;      // AcceptEx address buffer
        static constexpr INT clientIpBufferSize = 32;     // Client IP string buffer
    };
};
```

**Usage:**

```cpp
char buffer[Const::Buffer::recvBufferSize];
```

**Note:** These constants are baked into the binary at compile time. Modify `network/config.compile.toml` and regenerate via `scripts/parse_compile_cfg.ps1`.

---

## Platform Types

Windows platform type aliases defined in `network/platform.h`:

```cpp
using SocketHandle = SOCKET;           // Socket handle type
using SocketLength = int;              // Socket address length type
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;  // Invalid socket constant
constexpr int SocketError = SOCKET_ERROR;               // Socket error constant
```

**Example:**

```cpp
SocketHandle sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
if (sock == InvalidSocket) {
    logger->Error("Socket creation failed");
}
```

---

## Usage Examples

### Basic Echo Server

```cpp
#include <network/ServerLifecycle.h>
#include <network/WindowsAsyncSocket.h>
#include <Logger.hpp>
#include <TextLogger.h>

using namespace highp;

class EchoHandler : public network::IServerHandler {
    std::shared_ptr<log::Logger> _logger;

public:
    explicit EchoHandler(std::shared_ptr<log::Logger> logger)
        : _logger(logger) {}

    void OnAccept(std::shared_ptr<network::Client> client) override {
        _logger->Info("Client connected");
    }

    void OnRecv(std::shared_ptr<network::Client> client,
                std::span<const char> data) override {
        _logger->Debug("Received {} bytes", data.size());
        client->PostSend(std::string_view(data.data(), data.size()));
    }

    void OnSend(std::shared_ptr<network::Client> client,
                size_t bytesTransferred) override {
        _logger->Debug("Sent {} bytes", bytesTransferred);
    }

    void OnDisconnect(std::shared_ptr<network::Client> client) override {
        _logger->Info("Client disconnected");
    }
};

int main() {
    auto logger = log::Logger::Default<log::TextLogger>();
    auto config = network::NetworkCfg::FromFile("config.runtime.toml");

    EchoHandler handler(logger);
    network::ServerLifeCycle server(logger, &handler);

    auto listenSocket = std::make_shared<network::WindowsAsyncSocket>(logger);

    // Initialize and bind
    GUARD_VOID(listenSocket->Initialize());
    GUARD_VOID(listenSocket->CreateSocket(
        network::NetworkTransport(network::ETransport::TCP)));
    GUARD_VOID(listenSocket->Bind(config.server.port));
    GUARD_VOID(listenSocket->Listen(config.server.backlog));

    // Start server
    auto result = server.Start(listenSocket, config);
    if (result.HasErr()) {
        logger->Error("Server start failed: {}", err::ToString(result.Err()));
        return 1;
    }

    logger->Info("Server running on port {}", config.server.port);
    logger->Info("Press Enter to stop...");
    std::cin.get();

    server.Stop();
    logger->Info("Server stopped");

    return 0;
}
```

### Custom Completion Handler

```cpp
class MyServer {
    std::unique_ptr<network::IocpIoMultiplexer> _iocp;
    std::unique_ptr<network::IocpAcceptor> _acceptor;

    void OnCompletion(network::CompletionEvent event) {
        if (!event.success) {
            _logger->Error("I/O operation failed: {}", event.errorCode);
            HandleError(event);
            return;
        }

        switch (event.ioType) {
            case network::EIoType::Accept:
                HandleAccept(event);
                break;
            case network::EIoType::Recv:
                HandleRecv(event);
                break;
            case network::EIoType::Send:
                HandleSend(event);
                break;
            case network::EIoType::Disconnect:
                HandleDisconnect(event);
                break;
        }
    }

    void HandleRecv(network::CompletionEvent& event) {
        auto* client = static_cast<network::Client*>(event.completionKey);
        auto* recvOv = reinterpret_cast<network::RecvOverlapped*>(event.overlapped);

        if (event.bytesTransferred == 0) {
            // Graceful close
            CloseClient(client->shared_from_this());
            return;
        }

        // Process data
        std::span<const char> data(recvOv->buf, event.bytesTransferred);
        ProcessMessage(client->shared_from_this(), data);

        // Post next receive
        client->PostRecv();
    }

public:
    void Start() {
        _iocp = std::make_unique<network::IocpIoMultiplexer>(_logger);

        // Register completion handler
        _iocp->SetCompletionHandler([this](network::CompletionEvent event) {
            OnCompletion(event);
        });

        GUARD_VOID(_iocp->Initialize(std::thread::hardware_concurrency()));
        // ... continue setup
    }
};
```

### Object Pool Usage

```cpp
class CustomAcceptor {
    mem::ObjectPool<network::AcceptOverlapped> _pool;

public:
    CustomAcceptor() : _pool(10) {
        // Pre-allocate 10 overlapped structures
    }

    void PostAccept() {
        auto* ov = _pool.Acquire();

        // Initialize overlapped
        ZeroMemory(&ov->overlapped, sizeof(WSAOVERLAPPED));
        ov->ioType = network::EIoType::Accept;
        ov->clientSocket = CreateAcceptSocket();

        // Call AcceptEx
        BOOL result = AcceptEx(/* ... */, &ov->overlapped);

        // If synchronous completion, return to pool immediately
        if (result && WSAGetLastError() != WSA_IO_PENDING) {
            _pool.Release(ov);
        }
    }

    void OnAcceptComplete(network::AcceptOverlapped* ov) {
        // Process acceptance...

        // Return to pool
        _pool.Release(ov);
    }
};
```

### Error Handling Patterns

```cpp
// Pattern 1: Early return with GUARD
fn::Result<void, err::ENetworkError> InitializeServer() {
    GUARD(InitializeWinsock());
    GUARD(CreateListenSocket());
    GUARD(BindToPort(8080));
    GUARD(StartListening());
    return fn::Result<void, err::ENetworkError>::Ok();
}

// Pattern 2: Explicit error handling
auto result = client->PostRecv();
if (result.HasErr()) {
    _logger->Error("PostRecv failed: {}", err::ToString(result.Err()));
    CloseClient(client);
    return;
}

// Pattern 3: GUARD with side effect
GUARD_EFFECT(
    socket->Initialize(),
    [&]() { _logger->Error("Socket initialization failed"); }
);

// Pattern 4: Chained operations
auto InitChain() -> fn::Result<void, err::ENetworkError> {
    return socket->Initialize()
        .and_then([&]() { return socket->CreateSocket(transport); })
        .and_then([&]() { return socket->Bind(port); })
        .and_then([&]() { return socket->Listen(backlog); });
}
```

---

## Cross-References

- **Architecture Overview:** See `docs/ARCHITECTURE.md`
- **Network Layer Details:** See `docs/NETWORK_LAYER.md`
- **Configuration Guide:** See `docs/CONFIGURATION.md`
- **Protocol Specification:** See `docs/PROTOCOL.md`

---

## Notes on Thread Safety

### Thread-Safe Components

- `IocpIoMultiplexer::AssociateSocket()` - After initialization
- `IocpIoMultiplexer::PostCompletion()`
- `IocpIoMultiplexer::Shutdown()`
- `IocpAcceptor::PostAccept()` - After initialization
- `IocpAcceptor::PostAccepts()`
- `ServerLifeCycle::Stop()`
- `ServerLifeCycle::CloseClient()`
- `Client::PostRecv()`
- `Client::PostSend()`
- `Client::Close()`
- `ObjectPool<T>` - All methods
- `Logger` - All methods

### Not Thread-Safe

- All constructors
- `IocpIoMultiplexer::Initialize()`
- `IocpIoMultiplexer::SetCompletionHandler()`
- `IocpAcceptor::Initialize()`
- `IocpAcceptor::SetAcceptCallback()`
- `ServerLifeCycle::Start()`
- Socket initialization methods (`ISocket::Initialize()`, `CreateSocket()`, etc.)

### Thread Safety Guidelines

1. Perform all initialization from a single thread before starting workers
2. Register callbacks before initialization
3. Shutdown operations are thread-safe and idempotent
4. IOCP worker threads call `CompletionHandler` concurrently - handlers must be thread-safe
5. `IServerHandler` methods are called from worker threads - implementations must be thread-safe

---

**End of API Reference**


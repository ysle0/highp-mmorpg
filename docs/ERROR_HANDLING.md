# Error Handling Guide

This document describes the error handling philosophy and patterns used in the highp-mmorpg project.

## Table of Contents

1. [Error Handling Philosophy](#error-handling-philosophy)
2. [Result<T, E> Type](#resultt-e-type)
3. [ENetworkError Enum](#enetworkerror-enum)
4. [GUARD Macros](#guard-macros)
5. [Error Propagation Patterns](#error-propagation-patterns)
6. [Error Handling in Async Operations](#error-handling-in-async-operations)
7. [Logging Errors vs Propagating](#logging-errors-vs-propagating)
8. [Best Practices](#best-practices)
9. [Anti-Patterns](#anti-patterns)
10. [Testing Error Scenarios](#testing-error-scenarios)
11. [Complete Code Examples](#complete-code-examples)

---

## Error Handling Philosophy

### No Exceptions

This project **does not use C++ exceptions** for error handling. The rationale:

- **Predictable performance**: No hidden control flow or stack unwinding overhead
- **Explicit error handling**: Errors are visible in function signatures
- **Zero-overhead when successful**: No exception handling machinery
- **Deterministic behavior**: Critical for high-performance server applications

### Result<T, E> Monad

Instead of exceptions, we use the `Result<T, E>` type (similar to C++23's `std::expected` or Rust's `Result`):

- **Type-safe**: Compile-time enforcement of error handling
- **Composable**: Easy to chain operations with GUARD macros
- **Self-documenting**: Function signatures clearly indicate possible failures
- **[[nodiscard]]**: Compiler warnings if results are ignored

---

## Result<T, E> Type

### Overview

Located in `lib/Result.hpp`, the `Result<T, E>` type represents either success or failure:

```cpp
template <typename TData, typename E>
class [[nodiscard]] Result final;
```

### Template Parameters

- **TData**: The type of data returned on success
- **E**: The error type (typically an enum class like `ENetworkError`)

### Creating Results

```cpp
// Success with data
Result<int, ENetworkError> result = Result<int, ENetworkError>::Ok(42);

// Success without data (void specialization)
Result<void, ENetworkError> result = Result<void, ENetworkError>::Ok();

// Failure
Result<int, ENetworkError> result = Result<int, ENetworkError>::Err(ENetworkError::SocketCreateFailed);
```

### Checking Results

```cpp
Result<int, ENetworkError> res = SomeOperation();

// Method 1: IsOk() / HasErr()
if (res.IsOk()) {
    int value = res.Data();
    // Use value
}

if (res.HasErr()) {
    ENetworkError error = res.Err();
    // Handle error
}

// Method 2: bool operator (implicit conversion)
if (res) {
    // Success
} else {
    // Failure
}

// Method 3: Explicit bool operator
if (!res) {
    // Failure - propagate error
    return res;
}
```

### Result API Reference

#### Constructor Methods

| Method | Description |
|--------|-------------|
| `Result::Ok()` | Creates success result (void specialization) |
| `Result::Ok(TData data)` | Creates success result with data |
| `Result::Err(E err)` | Creates failure result with error code |

#### Query Methods

| Method | Return Type | Description |
|--------|-------------|-------------|
| `IsOk()` | `bool` | Returns true if operation succeeded |
| `HasErr()` | `bool` | Returns true if operation failed |
| `Data()` | `TData` | Returns success data (undefined if HasErr) |
| `Err()` | `E` | Returns error code (undefined if IsOk) |
| `operator bool()` | `bool` | Returns true if operation succeeded |

### void Specialization

When a function only needs to signal success/failure without returning data:

```cpp
template <typename E>
class [[nodiscard]] Result<void, E> final {
    // Only has error code, no data member
};

// Usage
Result<void, ENetworkError> Initialize() {
    if (/* initialization failed */) {
        return Result<void, ENetworkError>::Err(ENetworkError::WsaStartupFailed);
    }
    return Result<void, ENetworkError>::Ok();
}
```

### Type Aliases

Common pattern to reduce verbosity:

```cpp
class MyClass {
public:
    using Res = fn::Result<void, err::ENetworkError>;

    Res Initialize();
    Res Start();
};
```

---

## ENetworkError Enum

### Overview

Located in `lib/NetworkError.h`, defines all network-related error codes:

```cpp
namespace highp::err {
    enum class ENetworkError {
        UnknownError = 0,
        // WSA errors
        WsaStartupFailed,
        WsaNotInitialized,
        // Socket errors
        SocketInvalid,
        SocketCreateFailed,
        // IOCP errors
        IocpCreateFailed,
        IocpConnectFailed,
        // Thread errors
        ThreadWorkerCreateFailed,
        // ... and more
    };
}
```

### Complete Error Code Reference

#### WSA (Windows Sockets API) Errors

| Error Code | Description | When It Occurs |
|------------|-------------|----------------|
| `WsaStartupFailed` | WSAStartup() call failed | WSA initialization generic failure |
| `WsaNotInitialized` | WSAStartup() not called | WSA not initialized before use |
| `WsaNetworkSubSystemFailed` | Network subsystem failure | Network stack not operational |
| `WsaNotSupportedAddressFamily` | Unsupported address family | Invalid AF_INET/AF_INET6 |
| `WsaInvalidArgs` | Invalid arguments | Bad parameters to WSA function |
| `WsaLackFileDescriptor` | Too many open sockets | File descriptor limit reached |
| `WsaNoBufferSpace` | Insufficient buffer space | Memory allocation failed |
| `WsaNotSupportedProtocol` | Unsupported protocol | Protocol not available |
| `WsaRecvFailed` | WSARecv() failed | Async receive operation failed |
| `WsaSendFailed` | WSASend() failed | Async send operation failed |

#### Socket Errors

| Error Code | Description | When It Occurs |
|------------|-------------|----------------|
| `SocketInvalid` | Invalid socket handle | Using INVALID_SOCKET |
| `SocketCreateFailed` | Socket creation failed | WSASocketW() failed |
| `SocketBindFailed` | bind() failed | Address already in use or permission denied |
| `SocketListenFailed` | listen() failed | Socket not bound or invalid |
| `SocketAcceptFailed` | accept() failed | Generic accept error |
| `SocketPostAcceptFailed` | AcceptEx() failed | IOCP-specific accept failure |

#### IOCP Errors

| Error Code | Description | When It Occurs |
|------------|-------------|----------------|
| `IocpCreateFailed` | CreateIoCompletionPort() failed | IOCP handle creation failed |
| `IocpConnectFailed` | Socket-IOCP association failed | Failed to bind socket to IOCP |
| `IocpInternalError` | IOCP internal error | Unexpected IOCP state |
| `IocpIoFailed` | I/O operation failed | Generic I/O completion failure |

#### Thread Errors

| Error Code | Description | When It Occurs |
|------------|-------------|----------------|
| `ThreadWorkerCreateFailed` | Worker thread creation failed | std::jthread construction failed |
| `ThreadAccepterCreateFailed` | Accepter thread creation failed | Accept thread startup failed |
| `ThreadWorkerFailed` | Worker thread error | Runtime error in worker loop |
| `ThreadAcceptFailed` | Accept thread error | Runtime error in accept loop |

### ToString() Function

Convert error codes to human-readable messages:

```cpp
constexpr std::string_view ToString(ENetworkError e);

// Usage
auto err = ENetworkError::SocketCreateFailed;
std::string_view msg = ToString(err); // "Create socket failed."
```

### Error to String Mapping

```cpp
ENetworkError::WsaStartupFailed          -> "WSAStartup failed."
ENetworkError::WsaNotInitialized         -> "WSAStartup failed: WSAStartup() is not called."
ENetworkError::SocketCreateFailed        -> "Create socket failed."
ENetworkError::SocketBindFailed          -> "Bind failed."
ENetworkError::IocpCreateFailed          -> "CreateIoCompletionPort failed."
ENetworkError::ThreadWorkerCreateFailed  -> "Create worker thread failed."
// ... etc
```

---

## GUARD Macros

### Overview

Located in `lib/Result.hpp`, GUARD macros provide early-return error propagation similar to Rust's `?` operator:

```cpp
#define GUARD(expr)
#define GUARD_VOID(expr)
#define GUARD_BREAK(expr)
#define GUARD_EFFECT(expr, fn)
#define GUARD_EFFECT_VOID(expr, fn)
#define GUARD_EFFECT_BREAK(expr, fn)
```

### GUARD(expr)

**Purpose**: Propagate errors in functions returning `Result<T, E>`

**Signature**:
```cpp
#define GUARD(expr) \
do { \
    if (auto _result = (expr); _result.HasErr()) \
        return _result; \
} while (0)
```

**Usage**:
```cpp
Result<void, ENetworkError> Initialize() {
    GUARD(LoadAcceptExFunctions());  // If fails, return the error
    GUARD(PostAccept());              // If fails, return the error
    return Result::Ok();
}
```

**Expands to**:
```cpp
if (auto _result = LoadAcceptExFunctions(); _result.HasErr())
    return _result;
if (auto _result = PostAccept(); _result.HasErr())
    return _result;
return Result::Ok();
```

### GUARD_VOID(expr)

**Purpose**: Propagate errors in void functions (no return value)

**Signature**:
```cpp
#define GUARD_VOID(expr) \
do { \
    if (auto _result = (expr); _result.HasErr()) \
        return; \
} while (0)
```

**Usage**:
```cpp
void ProcessClient(Client* client) {
    GUARD_VOID(client->PostRecv());  // If fails, return early
    // Continue processing
}
```

### GUARD_BREAK(expr)

**Purpose**: Break from loop on error

**Signature**:
```cpp
#define GUARD_BREAK(expr) \
do { \
    if (auto _result = (expr); _result.HasErr()) \
        return; \
} while (0)
```

**Usage**:
```cpp
void ProcessBatch() {
    for (int i = 0; i < 10; ++i) {
        GUARD_BREAK(ProcessItem(i));  // Break loop on first error
    }
}
```

**Note**: Current implementation has `return` instead of `break` - this appears to be a bug in the codebase.

### GUARD_EFFECT(expr, fn)

**Purpose**: Execute cleanup/side-effect before propagating error

**Signature**:
```cpp
#define GUARD_EFFECT(expr, fn) \
do { \
    if (auto _result = (expr); _result.HasErr()) { \
        fn(); \
        return _result; \
    } \
} while (0)
```

**Usage**:
```cpp
Result<void, ENetworkError> ProcessRequest() {
    GUARD_EFFECT(
        SendData(),
        [&]() { logger->Error("Failed to send data"); }
    );
    return Result::Ok();
}
```

### GUARD_EFFECT_VOID(expr, fn)

**Purpose**: Execute side-effect before returning from void function

**Signature**:
```cpp
#define GUARD_EFFECT_VOID(expr, fn) \
do { \
    if (auto _result = (expr); _result.HasErr()) { \
        fn(); \
        return; \
    } \
} while (0)
```

**Usage**:
```cpp
void OnRecv(Client* client, std::span<const char> data) {
    GUARD_EFFECT_VOID(
        client->PostSend(data),
        [&]() { CloseClient(client); }
    );
}
```

### GUARD_EFFECT_BREAK(expr, fn)

**Purpose**: Execute side-effect before breaking from loop

**Signature**:
```cpp
#define GUARD_EFFECT_BREAK(expr, fn) \
do { \
    if (auto _result = (expr); _result.HasErr()) { \
        fn(); \
        break; \
    } \
} while (0)
```

### Comparison Table

| Macro | Return Type | On Error | Side Effect |
|-------|-------------|----------|-------------|
| `GUARD` | `Result<T, E>` | Return error | None |
| `GUARD_VOID` | `void` | Return | None |
| `GUARD_BREAK` | Any | Break/Return | None |
| `GUARD_EFFECT` | `Result<T, E>` | Return error | Execute `fn()` |
| `GUARD_EFFECT_VOID` | `void` | Return | Execute `fn()` |
| `GUARD_EFFECT_BREAK` | Any | Break | Execute `fn()` |

---

## Error Propagation Patterns

### Pattern 1: Simple Propagation (GUARD)

**When to use**: Propagating errors up the call stack without modification

```cpp
IocpAcceptor::Res IocpAcceptor::Initialize(SocketHandle listenSocket, HANDLE iocpHandle) {
    // Associate socket with IOCP
    GUARD(AssociateWithIocp(listenSocket, iocpHandle));

    // Load Windows extension functions
    GUARD(LoadAcceptExFunctions());

    _logger->Info("IocpAcceptor initialized.");
    return Res::Ok();
}
```

### Pattern 2: Manual Propagation with Logging

**When to use**: Need to add context or log before propagating

```cpp
IocpAcceptor::Res IocpAcceptor::PostAccept() {
    SocketHandle acceptSocket = CreateAcceptSocket();
    if (acceptSocket == InvalidSocket) {
        _logger->Error("Failed to create accept socket. error: {}", WSAGetLastError());
        return Res::Err(ENetworkError::SocketCreateFailed);
    }

    // Continue...
    return Res::Ok();
}
```

### Pattern 3: Error Transformation

**When to use**: Converting one error type to another

```cpp
ServerLifeCycle::Res ServerLifeCycle::Start(/* ... */) {
    if (auto res = _acceptor->Initialize(listenSocket, iocpHandle); res.HasErr()) {
        // Transform acceptor error to more specific error
        return Res::Err(ENetworkError::IocpCreateFailed);
    }
    return Res::Ok();
}
```

### Pattern 4: Error Handling with Recovery

**When to use**: Attempting recovery before propagating error

```cpp
void EchoServer::OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
    // Try to echo data back
    if (auto res = client->PostSend(data); res.HasErr()) {
        _logger->Error("Send failed for client #{}", client->socket);
        _core->CloseClient(client, true);  // Recovery: close the client
        return;  // Don't propagate - error is handled
    }

    // Continue processing
    if (auto res = client->PostRecv(); res.HasErr()) {
        _core->CloseClient(client, true);
    }
}
```

### Pattern 5: Multiple Error Checks in Sequence

**When to use**: Initialization sequences, multi-step operations

```cpp
ServerLifeCycle::Res ServerLifeCycle::Start(/* ... */) {
    // Step 1: Initialize IOCP
    if (auto res = _iocp->Initialize(workerCount); res.HasErr()) {
        return res;
    }

    // Step 2: Initialize Acceptor
    if (auto res = _acceptor->Initialize(listenSocket, iocpHandle); res.HasErr()) {
        return Res::Err(ENetworkError::IocpCreateFailed);
    }

    // Step 3: Post accept requests
    if (auto res = _acceptor->PostAccepts(backlog); res.HasErr()) {
        return Res::Err(ENetworkError::ThreadAcceptFailed);
    }

    return Res::Ok();
}
```

### Pattern 6: Loop with Early Exit

**When to use**: Processing batches where one failure should stop all

```cpp
IocpAcceptor::Res IocpAcceptor::PostAccepts(int count) {
    for (int i = 0; i < count; ++i) {
        GUARD(PostAccept());  // First failure stops the loop
    }
    _logger->Info("Posted {} AcceptEx requests.", count);
    return Res::Ok();
}
```

### Pattern 7: Nullable Return on Error

**When to use**: Factory functions, optional operations

```cpp
std::shared_ptr<ISocket> SocketHelper::MakeDefaultListener(/* ... */) {
    auto s = std::make_shared<WindowsAsyncSocket>(logger);

    if (auto res = s->Initialize(); res.HasErr()) {
        return nullptr;  // Can't use GUARD here
    }

    if (auto res = s->CreateSocket(netTransport); res.HasErr()) {
        return nullptr;
    }

    if (auto res = s->Bind(port); res.HasErr()) {
        return nullptr;
    }

    return s;
}
```

---

## Error Handling in Async Operations

### IOCP Completion Events

Async operations (AcceptEx, WSARecv, WSASend) complete via IOCP and require special error handling:

```cpp
void ServerLifeCycle::OnCompletion(CompletionEvent event) {
    // Check if I/O operation succeeded
    if (!event.success) {
        _logger->Error("I/O operation failed. error: {}", event.errorCode);
        // Handle based on operation type
        switch (event.ioType) {
            case EIoType::Accept:
                // Re-post accept
                break;
            case EIoType::Recv:
            case EIoType::Send:
                // Close client
                break;
        }
        return;
    }

    // Process successful completion
    switch (event.ioType) {
        case EIoType::Accept:
            OnAcceptInternal(event.overlapped);
            break;
        case EIoType::Recv:
            OnRecvInternal(event.completionKey, event.bytesTransferred);
            break;
        case EIoType::Send:
            OnSendInternal(event.completionKey, event.bytesTransferred);
            break;
    }
}
```

### Async Operation Error Flow

```
1. Post async operation (WSARecv/WSASend/AcceptEx)
   |
   +---> Returns SOCKET_ERROR
         |
         +---> WSAGetLastError() == ERROR_IO_PENDING
               |
               +---> Success! Operation is pending
               |
         +---> Other error code
               |
               +---> Immediate failure - return Result::Err

2. IOCP completion
   |
   +---> GetQueuedCompletionStatus returns FALSE
         |
         +---> Operation failed - check event.errorCode
               |
               +---> Handle error, possibly close connection
   |
   +---> GetQueuedCompletionStatus returns TRUE
         |
         +---> Success! Process event.bytesTransferred
```

### Pattern: Posting Async Operations

```cpp
Client::Res Client::PostRecv() {
    ZeroMemory(&recvOverlapped.overlapped, sizeof(WSAOVERLAPPED));
    recvOverlapped.bufDesc.buf = recvOverlapped.buf;
    recvOverlapped.bufDesc.len = std::size(recvOverlapped.buf);
    recvOverlapped.ioType = EIoType::Recv;

    DWORD flags = 0;
    DWORD recvNumBytes = 0;

    int result = WSARecv(
        socket,
        &recvOverlapped.bufDesc,
        1,
        &recvNumBytes,
        &flags,
        reinterpret_cast<LPWSAOVERLAPPED>(&recvOverlapped),
        nullptr
    );

    // ERROR_IO_PENDING is success - operation will complete later
    if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
        return Res::Err(ENetworkError::WsaRecvFailed);
    }

    return Res::Ok();
}
```

### Pattern: Processing Completions

```cpp
void IocpIoMultiplexer::WorkerLoop(std::stop_token st) {
    while (!st.stop_requested() && _isRunning) {
        DWORD bytesTransferred = 0;
        void* completionKey = nullptr;
        LPOVERLAPPED overlapped = nullptr;

        BOOL success = GetQueuedCompletionStatus(
            _handle,
            &bytesTransferred,
            reinterpret_cast<PULONG_PTR>(&completionKey),
            &overlapped,
            INFINITE
        );

        // Check for shutdown signal
        if (overlapped == nullptr && completionKey == nullptr) {
            break;
        }

        // Build completion event
        CompletionEvent event{
            .ioType = /* from overlapped */,
            .completionKey = completionKey,
            .overlapped = overlapped,
            .bytesTransferred = bytesTransferred,
            .success = (success == TRUE),
            .errorCode = success ? 0 : GetLastError()
        };

        // Dispatch to handler
        if (_completionHandler) {
            _completionHandler(event);
        }
    }
}
```

---

## Logging Errors vs Propagating

### When to Log

**Log at the point of failure** when you have the most context:

```cpp
IocpAcceptor::Res IocpAcceptor::LoadAcceptExFunctions() {
    GUID guidAcceptEx = WSAID_ACCEPTEX;
    DWORD bytes = 0;

    int result = WSAIoctl(/* ... */);

    if (result == SOCKET_ERROR) {
        // Log with full context including WSA error code
        _logger->Error("Failed to load AcceptEx function. error: {}", WSAGetLastError());
        return Res::Err(ENetworkError::SocketPostAcceptFailed);
    }

    return Res::Ok();
}
```

### When to Propagate Silently

**Propagate without logging** when caller has better context:

```cpp
IocpAcceptor::Res IocpAcceptor::Initialize(SocketHandle listenSocket, HANDLE iocpHandle) {
    // Don't log here - LoadAcceptExFunctions already logged
    GUARD(LoadAcceptExFunctions());

    _logger->Info("IocpAcceptor initialized.");
    return Res::Ok();
}
```

### When to Log AND Propagate

**Log at intermediate layers** to provide context at each abstraction level:

```cpp
// Low level - logs WSA error code
Client::Res Client::PostSend(std::string_view data) {
    int result = WSASend(/* ... */);

    if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
        // Could log here with low-level details
        return Res::Err(ENetworkError::WsaSendFailed);
    }
    return Res::Ok();
}

// High level - logs business context
void EchoServer::OnRecv(std::shared_ptr<Client> client, std::span<const char> data) {
    if (auto res = client->PostSend(data); res.HasErr()) {
        // Log with business context
        _logger->Error("Failed to echo data for client #{}", client->socket);
        _core->CloseClient(client, true);
        return;
    }
}
```

### Helper Functions (lib/Errors.hpp)

The codebase provides helper functions for common log+return patterns:

```cpp
namespace highp::err {
    // Log WSA error and return Result
    template <auto E>
    fn::Result<void, decltype(E)> LogErrorWSAWithResult(std::shared_ptr<log::Logger> logger);

    // Log Windows error and return Result
    template <auto E>
    fn::Result<void, decltype(E)> LogErrorWindowsWithResult(std::shared_ptr<log::Logger> logger);

    // Log error message and return Result
    template <auto E>
    fn::Result<void, decltype(E)> LogErrorWithResult(std::shared_ptr<log::Logger> logger);
}

// Usage
if (socket == INVALID_SOCKET) {
    return LogErrorWSAWithResult<ENetworkError::SocketCreateFailed>(_logger);
}
```

### Logging Guidelines

| Scenario | Log? | Propagate? | Rationale |
|----------|------|------------|-----------|
| Low-level OS error | Yes | Yes | Need raw error codes (WSAGetLastError) |
| Initialization failure | Yes | Yes | Critical path, need full trace |
| Expected failure (client disconnect) | No | Yes | Normal operation, too verbose |
| Unexpected state | Yes | Yes | Indicates bug, need debugging info |
| Business logic error | Yes | Maybe | Application-level context needed |
| Retry-able operation | No | Yes | Will retry, don't spam logs |

---

## Best Practices

### 1. Always Use [[nodiscard]]

The `Result` type is marked `[[nodiscard]]` - never ignore return values:

```cpp
// BAD - compiler warning!
ValidateInput();  // Result ignored

// GOOD
if (auto res = ValidateInput(); res.HasErr()) {
    return res;
}

// GOOD - with GUARD
GUARD(ValidateInput());
```

### 2. Prefer GUARD for Simple Cases

GUARD macros reduce boilerplate and improve readability:

```cpp
// Without GUARD - verbose
Result<void, ENetworkError> Initialize() {
    if (auto res = Step1(); res.HasErr()) {
        return res;
    }
    if (auto res = Step2(); res.HasErr()) {
        return res;
    }
    if (auto res = Step3(); res.HasErr()) {
        return res;
    }
    return Result::Ok();
}

// With GUARD - concise
Result<void, ENetworkError> Initialize() {
    GUARD(Step1());
    GUARD(Step2());
    GUARD(Step3());
    return Result::Ok();
}
```

### 3. Use Type Aliases

Define `Res` alias to reduce verbosity:

```cpp
class ServerLifeCycle {
public:
    using Res = fn::Result<void, err::ENetworkError>;

    Res Start(/* ... */);
    Res Stop();
};
```

### 4. Check ERROR_IO_PENDING

For async operations, `ERROR_IO_PENDING` is **not an error**:

```cpp
int result = WSARecv(/* ... */);

// CORRECT - ERROR_IO_PENDING is success
if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
    return Res::Err(ENetworkError::WsaRecvFailed);
}

// WRONG - will fail on pending operations
if (result == SOCKET_ERROR) {
    return Res::Err(ENetworkError::WsaRecvFailed);
}
```

### 5. Log at the Right Level

Follow the logging pyramid:

```
        ERROR (rare)        - Failures requiring attention
       /                \
      WARN (occasional)     - Degraded operation, retries
     /                    \
    INFO (moderate)          - Lifecycle events, connections
   /                        \
  DEBUG (verbose)              - Detailed operation traces
```

### 6. Fail Fast in Constructors

Since constructors can't return `Result`, use factory methods:

```cpp
class IocpAcceptor {
public:
    // Factory method with error handling
    static Result<std::unique_ptr<IocpAcceptor>, ENetworkError> Create(/* ... */) {
        auto acceptor = std::make_unique<IocpAcceptor>(/* ... */);
        GUARD(acceptor->Initialize());
        return Result::Ok(std::move(acceptor));
    }

private:
    IocpAcceptor(/* ... */);  // Private constructor
    Res Initialize();
};
```

### 7. Document Error Conditions

Comment when errors are expected vs unexpected:

```cpp
// Expected: Client can disconnect at any time
if (bytesTransferred == 0) {
    _logger->Info("Client disconnected gracefully");
    return;
}

// Unexpected: Should never happen if properly initialized
if (_iocpHandle == INVALID_HANDLE_VALUE) {
    _logger->Error("IOCP handle is invalid - initialization bug!");
    return Res::Err(ENetworkError::IocpInternalError);
}
```

### 8. Clean Up on Error Paths

Use RAII or explicit cleanup:

```cpp
IocpAcceptor::Res IocpAcceptor::PostAccept() {
    OverlappedExt* overlapped = AcquireOverlapped();
    SocketHandle acceptSocket = CreateAcceptSocket();

    if (acceptSocket == InvalidSocket) {
        ReleaseOverlapped(overlapped);  // Clean up before returning
        _logger->Error("Failed to create accept socket");
        return Res::Err(ENetworkError::SocketCreateFailed);
    }

    BOOL result = _fnAcceptEx(/* ... */);

    if (result == FALSE && WSAGetLastError() != ERROR_IO_PENDING) {
        closesocket(acceptSocket);      // Clean up socket
        ReleaseOverlapped(overlapped);   // Clean up overlapped
        return Res::Err(ENetworkError::SocketPostAcceptFailed);
    }

    return Res::Ok();
}
```

### 9. Return Early, Return Often

Minimize nesting by handling errors early:

```cpp
// GOOD - flat structure
Result<void, ENetworkError> Process() {
    if (condition1) return Res::Err(/* ... */);
    if (condition2) return Res::Err(/* ... */);
    if (condition3) return Res::Err(/* ... */);

    // Happy path
    DoWork();
    return Res::Ok();
}

// BAD - deep nesting
Result<void, ENetworkError> Process() {
    if (!condition1) {
        if (!condition2) {
            if (!condition3) {
                DoWork();
                return Res::Ok();
            } else {
                return Res::Err(/* ... */);
            }
        } else {
            return Res::Err(/* ... */);
        }
    } else {
        return Res::Err(/* ... */);
    }
}
```

### 10. Test Error Paths

Ensure error handling is tested:

```cpp
// Test initialization failure
void TestIocpInitFailure() {
    auto iocp = std::make_unique<IocpIoMultiplexer>(logger, handler);

    // Force failure by passing invalid worker count
    auto res = iocp->Initialize(-1);

    assert(res.HasErr());
    assert(res.Err() == ENetworkError::ThreadWorkerCreateFailed);
}
```

---

## Anti-Patterns

### 1. Ignoring Results

```cpp
// BAD - ignores potential failure
(void)Initialize();  // Silencing [[nodiscard]]

// GOOD
if (auto res = Initialize(); res.HasErr()) {
    _logger->Error("Initialization failed");
    return res;
}
```

### 2. Catching Errors Too Early

```cpp
// BAD - loses error context
Result<void, ENetworkError> HighLevel() {
    if (auto res = LowLevel(); res.HasErr()) {
        // Generic error - lost specific error code
        return Res::Err(ENetworkError::UnknownError);
    }
    return Res::Ok();
}

// GOOD - preserve error
Result<void, ENetworkError> HighLevel() {
    GUARD(LowLevel());  // Propagates original error
    return Res::Ok();
}
```

### 3. Using Exceptions

```cpp
// BAD - this project doesn't use exceptions!
void DoWork() {
    if (failed) {
        throw std::runtime_error("Operation failed");
    }
}

// GOOD
Result<void, ENetworkError> DoWork() {
    if (failed) {
        return Res::Err(ENetworkError::IocpIoFailed);
    }
    return Res::Ok();
}
```

### 4. Returning Error Codes Directly

```cpp
// BAD - using raw int error codes
int Initialize() {
    if (failed) return -1;
    return 0;
}

// GOOD - using Result
Result<void, ENetworkError> Initialize() {
    if (failed) return Res::Err(ENetworkError::WsaStartupFailed);
    return Res::Ok();
}
```

### 5. Not Checking Async Operation Results

```cpp
// BAD - assuming PostRecv always succeeds
void OnAccept(Client* client) {
    client->PostRecv();  // May fail!
}

// GOOD
void OnAccept(Client* client) {
    if (auto res = client->PostRecv(); res.HasErr()) {
        _logger->Error("Failed to post recv for client");
        CloseClient(client);
    }
}
```

### 6. Over-Logging

```cpp
// BAD - logging at every layer
Result<void, ENetworkError> Layer3() {
    if (auto res = Layer2(); res.HasErr()) {
        _logger->Error("Layer3 failed");  // Redundant
        return res;
    }
    return Res::Ok();
}

// GOOD - log once at point of failure
Result<void, ENetworkError> Layer1() {
    if (failed) {
        _logger->Error("WSARecv failed: {}", WSAGetLastError());
        return Res::Err(ENetworkError::WsaRecvFailed);
    }
    return Res::Ok();
}

Result<void, ENetworkError> Layer3() {
    GUARD(Layer2());  // Propagate silently
    return Res::Ok();
}
```

### 7. Magic Error Numbers

```cpp
// BAD - what does 0 mean?
enum class ENetworkError {
    UnknownError = 0,
    SocketCreateFailed = 42,  // Random number
};

// GOOD - semantic naming
enum class ENetworkError {
    UnknownError = 0,         // Default/fallback
    SocketCreateFailed,       // Auto-increment
};
```

### 8. Forgetting ERROR_IO_PENDING

```cpp
// BAD - treats pending as error
if (WSARecv(...) == SOCKET_ERROR) {
    return Res::Err(ENetworkError::WsaRecvFailed);
}

// GOOD
if (WSARecv(...) == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
    return Res::Err(ENetworkError::WsaRecvFailed);
}
```

---

## Testing Error Scenarios

### Manual Testing Strategy

Since the project uses manual integration testing, error scenarios should be tested through:

1. **Invalid Configuration**
2. **Resource Exhaustion**
3. **Network Failures**
4. **Concurrent Operations**

### Test Case Examples

#### 1. WSA Initialization Failure

```cpp
// Test: Start server without WSAStartup
void TestWsaNotInitialized() {
    // Don't call WSAStartup
    auto logger = Logger::Default<TextLogger>();
    auto socket = std::make_shared<WindowsAsyncSocket>(logger);

    // Should fail with WsaNotInitialized
    auto res = socket->CreateSocket(transport);
    assert(res.HasErr());
    assert(res.Err() == ENetworkError::WsaNotInitialized);
}
```

#### 2. Port Already in Use

```powershell
# Terminal 1: Start first server
.\x64\Debug\echo-server.exe

# Terminal 2: Start second server (same port)
.\x64\Debug\echo-server.exe  # Should fail with SocketBindFailed
```

#### 3. IOCP Worker Thread Limit

```cpp
// Test: Request too many worker threads
void TestTooManyWorkers() {
    auto iocp = std::make_unique<IocpIoMultiplexer>(logger, handler);

    // Try to create excessive threads
    auto res = iocp->Initialize(10000);

    // Should fail with ThreadWorkerCreateFailed
    assert(res.HasErr());
}
```

#### 4. Client Pool Exhaustion

Modify `config.runtime.toml`:
```toml
[server]
max_clients = 2  # Very low limit
```

Then connect 3+ clients - the 3rd should fail gracefully.

#### 5. Socket Close During I/O

```cpp
// Simulate: Close socket while recv is pending
void TestCloseDuringIo() {
    auto client = std::make_shared<Client>();
    client->PostRecv();

    // Immediately close
    client->Close(true);

    // IOCP completion should handle gracefully
}
```

#### 6. Invalid Overlapped Pointer

```cpp
// Test: Pass corrupted overlapped to completion handler
void TestInvalidOverlapped() {
    CompletionEvent event{
        .overlapped = nullptr,  // Invalid!
        .success = false
    };

    // Handler should check for null and handle gracefully
    OnCompletion(event);
}
```

### Error Injection Techniques

#### Technique 1: Configuration Overrides

Use environment variables to force errors:

```powershell
# Force bind to privileged port (requires admin)
$env:SERVER_PORT = "80"
.\x64\Debug\echo-server.exe  # Should fail with SocketBindFailed
```

#### Technique 2: Resource Limits

Modify compile-time config:

```toml
# config.compile.toml
[buffer]
recv_buffer_size = 0  # Invalid size
send_buffer_size = 0
```

#### Technique 3: Debugger Breakpoints

Set breakpoints before critical operations and modify variables:

```cpp
// In debugger, set breakpoint here
BOOL result = AcceptEx(/* ... */);

// Manually set result = FALSE in debugger to simulate failure
```

### Validation Checklist

For each error code in `ENetworkError`, verify:

- [ ] Error is returned in at least one code path
- [ ] Error has appropriate `ToString()` message
- [ ] Logging includes system error codes (WSAGetLastError/GetLastError)
- [ ] Resources are cleaned up on error path
- [ ] Error propagates to appropriate layer
- [ ] Retry logic (if applicable) works correctly
- [ ] Error doesn't cause crash or undefined behavior

---

## Complete Code Examples

### Example 1: Full Initialization Sequence

```cpp
// ServerLifecycle.cpp - Complete startup with error handling

namespace highp::network {

ServerLifeCycle::Res ServerLifeCycle::Start(
    std::shared_ptr<ISocket> listenSocket,
    const NetworkCfg& config
) {
    _config = config;

    // ========== Step 1: IOCP Initialization ==========
    _iocp = std::make_unique<IocpIoMultiplexer>(
        _logger,
        std::bind_front(&ServerLifeCycle::OnCompletion, this)
    );

    const auto workerCount = std::thread::hardware_concurrency() *
        _config.thread.maxWorkerThreadMultiplier;

    // GUARD propagates error if IOCP init fails
    if (auto res = _iocp->Initialize(workerCount); res.HasErr()) {
        return res;  // Returns original error code
    }

    // ========== Step 2: Acceptor Initialization ==========
    _acceptor = std::make_unique<IocpAcceptor>(
        _logger,
        _config.server.backlog,
        std::bind_front(&ServerLifeCycle::OnAcceptInternal, this)
    );

    // Transform acceptor error to more specific error
    if (auto res = _acceptor->Initialize(listenSocket->GetSocketHandle(), _iocp->GetHandle());
        res.HasErr()) {
        return Res::Err(ENetworkError::IocpCreateFailed);
    }

    // ========== Step 3: Pre-allocate Client Pool ==========
    _clientPool.reserve(_config.server.maxClients);
    for (int i = 0; i < _config.server.maxClients; ++i) {
        _clientPool.emplace_back(std::make_shared<Client>());
    }

    // ========== Step 4: Post Initial Accept Requests ==========
    if (auto res = _acceptor->PostAccepts(_config.server.backlog); res.HasErr()) {
        return Res::Err(ENetworkError::ThreadAcceptFailed);
    }

    _logger->Info("Server started on port {}.", _config.server.port);
    return Res::Ok();
}

} // namespace highp::network
```

### Example 2: Async Operation Chain

```cpp
// EchoServer.cpp - Async recv -> send -> recv loop

namespace highp::echo_srv {

void EchoServer::OnRecv(
    std::shared_ptr<network::Client> client,
    std::span<const char> data
) {
    std::string_view recvData{ data.data(), data.size() };

    _logger->Info("[EchoServer] Recv: socket #{}, data: {}, bytes: {}",
        client->socket, recvData, data.size());

    // ========== Step 1: Echo data back to client ==========
    if (auto res = client->PostSend(recvData); res.HasErr()) {
        // Send failed - log and close connection
        _logger->Error("[EchoServer] PostSend failed for socket #{}", client->socket);
        _core->CloseClient(client, true);
        return;  // Don't propagate - error is handled
    }

    // ========== Step 2: Post next receive ==========
    if (auto res = client->PostRecv(); res.HasErr()) {
        // Recv post failed - log and close connection
        _logger->Error("[EchoServer] PostRecv failed for socket #{}", client->socket);
        _core->CloseClient(client, true);
        return;
    }

    // Both operations posted successfully - will complete async
}

void EchoServer::OnSend(
    std::shared_ptr<network::Client> client,
    size_t bytesTransferred
) {
    _logger->Info("[EchoServer] Send: socket #{}, bytes: {}",
        client->socket, bytesTransferred);

    // Send completed successfully - no action needed
    // Next recv will be posted from OnRecv handler
}

} // namespace highp::echo_srv
```

### Example 3: Resource Management with Cleanup

```cpp
// IocpAcceptor.cpp - AcceptEx with proper resource cleanup

namespace highp::network {

IocpAcceptor::Res IocpAcceptor::PostAccept() {
    // ========== Acquire Resources ==========
    OverlappedExt* overlapped = AcquireOverlapped();
    SocketHandle acceptSocket = CreateAcceptSocket();

    // ========== Validate Socket ==========
    if (acceptSocket == InvalidSocket) {
        ReleaseOverlapped(overlapped);  // Clean up before error return
        _logger->Error("Failed to create accept socket. error: {}", WSAGetLastError());
        return Res::Err(ENetworkError::SocketCreateFailed);
    }

    // ========== Setup Overlapped Structure ==========
    ZeroMemory(&overlapped->overlapped, sizeof(WSAOVERLAPPED));
    overlapped->ioType = EIoType::Accept;
    overlapped->clientSocket = acceptSocket;

    // ========== Post AcceptEx ==========
    DWORD bytesReceived = 0;
    constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;

    BOOL result = _fnAcceptEx(
        _listenSocket,
        acceptSocket,
        overlapped->sendBuffer,
        0,
        addrLen,
        addrLen,
        &bytesReceived,
        reinterpret_cast<LPOVERLAPPED>(overlapped)
    );

    // ========== Check Result ==========
    if (result == FALSE) {
        DWORD err = WSAGetLastError();

        // ERROR_IO_PENDING is success - operation will complete later
        if (err != ERROR_IO_PENDING) {
            // Real error - clean up resources
            closesocket(acceptSocket);
            ReleaseOverlapped(overlapped);

            _logger->Error("AcceptEx failed. error: {}", err);
            return Res::Err(ENetworkError::SocketPostAcceptFailed);
        }
    }

    // Success - resources will be cleaned up on completion
    return Res::Ok();
}

} // namespace highp::network
```

### Example 4: Batch Operations with Early Exit

```cpp
// IocpAcceptor.cpp - Post multiple accepts

namespace highp::network {

IocpAcceptor::Res IocpAcceptor::PostAccepts(int count) {
    // Post multiple accept requests
    // If any fails, stop and return error
    for (int i = 0; i < count; ++i) {
        GUARD(PostAccept());  // First failure stops the loop
    }

    _logger->Info("Posted {} AcceptEx requests.", count);
    return Res::Ok();
}

} // namespace highp::network
```

### Example 5: Factory Pattern with Error Handling

```cpp
// SocketHelper.cpp - Socket factory with multi-step initialization

namespace highp::network {

std::shared_ptr<ISocket> SocketHelper::MakeDefaultListener(
    std::shared_ptr<log::Logger> logger,
    NetworkTransport netTransport,
    NetworkCfg networkCfg,
    std::shared_ptr<SocketOptionBuilder> socketOptionBuilder
) {
    // Create socket instance
    auto s = std::make_shared<WindowsAsyncSocket>(logger);

    // ========== WSA Initialization ==========
    if (auto res = s->Initialize(); res.HasErr()) {
        // Initialization failed - can't use GUARD with nullable return
        logger->Error("Failed to initialize socket");
        return nullptr;
    }

    // ========== Socket Creation ==========
    if (auto res = s->CreateSocket(netTransport); res.HasErr()) {
        logger->Error("Failed to create socket");
        return nullptr;
    }

    // ========== Socket Options ==========
    const SocketHandle sh = s->GetSocketHandle();
    socketOptionBuilder->SetReuseAddr(sh, true);

    // ========== Bind ==========
    if (auto res = s->Bind(networkCfg.server.port); res.HasErr()) {
        logger->Error("Failed to bind socket to port {}", networkCfg.server.port);
        return nullptr;
    }

    // ========== Listen ==========
    if (auto res = s->Listen(networkCfg.server.backlog); res.HasErr()) {
        logger->Error("Failed to listen on socket");
        return nullptr;
    }

    // All steps succeeded
    return s;
}

} // namespace highp::network
```

### Example 6: IOCP Completion Handler

```cpp
// ServerLifecycle.cpp - Complete IOCP event handling

namespace highp::network {

void ServerLifeCycle::OnCompletion(CompletionEvent event) {
    // ========== Check for I/O Failure ==========
    if (!event.success) {
        _logger->Error("I/O operation failed. error: {}", event.errorCode);

        // Handle based on operation type
        switch (event.ioType) {
            case EIoType::Accept:
                // Re-post accept to keep accepting connections
                if (auto res = _acceptor->PostAccept(); res.HasErr()) {
                    _logger->Error("Failed to re-post accept");
                }
                break;

            case EIoType::Recv:
            case EIoType::Send: {
                // Close failed client connection
                auto client = static_cast<Client*>(event.completionKey);
                if (client) {
                    CloseClient(std::shared_ptr<Client>(client), true);
                }
                break;
            }

            default:
                _logger->Error("Unknown IO type received.");
                break;
        }
        return;
    }

    // ========== Dispatch Successful Completion ==========
    switch (event.ioType) {
        case EIoType::Accept:
            OnAcceptInternal(event.overlapped);
            break;

        case EIoType::Recv:
            OnRecvInternal(event.completionKey, event.bytesTransferred);
            break;

        case EIoType::Send:
            OnSendInternal(event.completionKey, event.bytesTransferred);
            break;

        default:
            _logger->Error("Unknown IO type received.");
            break;
    }
}

void ServerLifeCycle::OnAcceptInternal(LPOVERLAPPED overlapped) {
    // Cast overlapped back to our extended structure
    auto* overlappedExt = reinterpret_cast<OverlappedExt*>(overlapped);

    // ========== Acquire Client from Pool ==========
    auto client = AcquireClient();
    if (!client) {
        _logger->Error("Client pool full!");
        closesocket(overlappedExt->clientSocket);
        _acceptor->ReleaseOverlapped(overlappedExt);

        // Re-post accept even though pool is full
        if (auto res = _acceptor->PostAccept(); res.HasErr()) {
            _logger->Error("Failed to re-post accept");
        }
        return;
    }

    // ========== Setup Client ==========
    client->socket = overlappedExt->clientSocket;
    _connectedClientCount++;

    // ========== Associate with IOCP ==========
    if (auto res = _iocp->AssociateSocket(client->socket, client.get()); res.HasErr()) {
        _logger->Error("Failed to associate socket with IOCP.");
        CloseClient(client, true);
        _acceptor->ReleaseOverlapped(overlappedExt);
        return;
    }

    // ========== Post Initial Receive ==========
    if (auto res = client->PostRecv(); res.HasErr()) {
        _logger->Error("Failed to post initial Recv.");
        CloseClient(client, true);
        _acceptor->ReleaseOverlapped(overlappedExt);
        return;
    }

    // ========== Notify Application ==========
    if (_handler) {
        _handler->OnAccept(client);
    }

    // ========== Cleanup and Re-post ==========
    _acceptor->ReleaseOverlapped(overlappedExt);

    // Re-post accept to continue accepting connections
    if (auto res = _acceptor->PostAccept(); res.HasErr()) {
        _logger->Error("Failed to re-post AcceptEx after completion.");
    }
}

} // namespace highp::network
```

### Example 7: Main Entry Point

```cpp
// main.cpp - Application entry with error handling

#include "EchoServer.h"
#include <iostream>
#include <Logger.hpp>
#include <NetworkCfg.h>
#include <SocketHelper.h>
#include <TextLogger.h>

using namespace highp::echo_srv;
using namespace highp::network;

int main() {
    // ========== Setup ==========
    auto logger = log::Logger::Default<log::TextLogger>();
    auto config = NetworkCfg::FromFile("config.runtime.toml");
    auto transport = NetworkTransport{ ETransport::TCP };
    auto socketOptionBuilder = std::make_shared<SocketOptionBuilder>(logger);

    // ========== Create Listen Socket ==========
    auto listenSocket = SocketHelper::MakeDefaultListener(
        logger,
        transport,
        config,
        socketOptionBuilder
    );

    if (!listenSocket) {
        logger->Error("Failed to create listen socket.");
        return -1;
    }

    // ========== Start Server ==========
    EchoServer server(logger, config, socketOptionBuilder);

    if (auto res = server.Start(listenSocket); res.HasErr()) {
        logger->Error("Failed to start server. error: {}",
            err::ToString(res.Err()));
        return -1;
    }

    // ========== Wait for Shutdown ==========
    logger->Info("Press Enter to stop the server...");
    std::cin.get();

    // ========== Cleanup ==========
    server.Stop();
    logger->Info("Server stopped successfully.");

    return 0;
}
```

---

## Summary

This document covers the comprehensive error handling strategy for the highp-mmorpg project:

- **No exceptions**: All errors use `Result<T, E>` monad for type-safe, explicit error handling
- **GUARD macros**: Provide concise error propagation with early returns
- **ENetworkError enum**: Categorizes all network-related errors with descriptive messages
- **Async operations**: Special handling for IOCP completions and `ERROR_IO_PENDING`
- **Logging strategy**: Log at the point of failure with full context, propagate silently upward
- **Best practices**: [[nodiscard]] enforcement, RAII cleanup, early returns
- **Anti-patterns**: Avoid ignoring results, losing error context, or over-logging

The Result-based error handling provides:
- Compile-time safety
- Zero overhead when successful
- Explicit error paths
- Excellent composability
- Clear documentation through types

For questions or clarifications, refer to:
- `lib/Result.hpp` - Result type implementation
- `lib/NetworkError.h` - Error code definitions
- `lib/Errors.hpp` - Error helper functions
- `network/` - Real-world usage examples


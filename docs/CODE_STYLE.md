# Code Style Guide

This document defines the coding conventions used in the highp-mmorpg project. These conventions are derived from the existing codebase in the `network/` and `lib/` directories.

## Table of Contents

1. [Naming Conventions](#naming-conventions)
2. [File Organization and Structure](#file-organization-and-structure)
3. [Header File Best Practices](#header-file-best-practices)
4. [Comment and Documentation Style](#comment-and-documentation-style)
5. [Code Formatting Guidelines](#code-formatting-guidelines)
6. [Memory Management Patterns](#memory-management-patterns)
7. [Error Handling Style](#error-handling-style)
8. [Threading and Concurrency Patterns](#threading-and-concurrency-patterns)
9. [Platform-Specific Code Guidelines](#platform-specific-code-guidelines)
10. [Examples of Good vs Bad Style](#examples-of-good-vs-bad-style)

---

## Naming Conventions

### Types and Classes

Use **PascalCase** for all type names, classes, structs, and enums:

```cpp
// Good
class IocpAcceptor { };
struct Client { };
enum class ETransport { };
template <typename T> class ObjectPool { };

// Bad
class iocp_acceptor { };
struct client { };
enum class transport { };
```

### Enums

Prefix enum class names with `E`:

```cpp
// Good
enum class ETransport {
    TCP,
    UDP,
};

enum class EIoType {
    Accept,
    Recv,
    Send,
};

// Bad
enum class Transport { };
enum class IoType { };
```

### Methods and Functions

Use **PascalCase** for all public methods and free functions:

```cpp
// Good
void Initialize();
Result PostAccept();
bool IsValidPort(unsigned short port);

// Bad
void initialize();
Result post_accept();
bool is_valid_port(unsigned short port);
```

### Member Variables

Use **camelCase** with leading underscore (`_`) for private/protected member variables:

```cpp
// Good
class IocpAcceptor {
private:
    std::shared_ptr<log::Logger> _logger;
    SocketHandle _listenSocket;
    HANDLE _iocpHandle;
};

// Bad
class IocpAcceptor {
private:
    std::shared_ptr<log::Logger> Logger;
    SocketHandle listen_socket;
    HANDLE m_iocpHandle;
};
```

Public struct members do not use leading underscore:

```cpp
// Good
struct Client {
    SocketHandle socket = INVALID_SOCKET;
    RecvOverlapped recvOverlapped;
    SendOverlapped sendOverlapped;
};
```

### Function Parameters

Use **camelCase** for function parameters:

```cpp
// Good
void PostAccepts(int count);
Result Initialize(SocketHandle listenSocket, HANDLE iocpHandle);

// Bad
void PostAccepts(int Count);
Result Initialize(SocketHandle ListenSocket, HANDLE IOCPHandle);
```

### Local Variables

Use **camelCase** for local variables:

```cpp
// Good
auto workerCount = std::thread::hardware_concurrency();
SocketHandle acceptSocket = CreateAcceptSocket();

// Bad
auto WorkerCount = std::thread::hardware_concurrency();
SocketHandle AcceptSocket = CreateAcceptSocket();
```

### Constants and Compile-Time Configuration

Use **snake_case** for TOML configuration keys and constants within namespace scopes:

```toml
# Good - config.compile.toml
[buffer]
recv_buffer_size = 4096
send_buffer_size = 1024
client_ip_buffer_size = 32

# Bad
[buffer]
RecvBufferSize = 4096
SendBufferSize = 1024
```

Generated constants use PascalCase within namespaces:

```cpp
namespace Const {
    namespace Buffer {
        constexpr int recvBufferSize = 4096;
        constexpr int sendBufferSize = 1024;
    }
}
```

### Type Aliases

Use **PascalCase** for type aliases:

```cpp
// Good
using Res = fn::Result<void, err::ENetworkError>;
using AcceptCallback = std::function<void(AcceptContext&)>;
using NetworkTransport = WindowsNetworkTransport;

// Bad
using res = fn::Result<void, err::ENetworkError>;
using accept_callback = std::function<void(AcceptContext&)>;
```

### Template Parameters

Use **PascalCase** with descriptive prefixes:

```cpp
// Good
template <typename TData, typename E>
class Result { };

template <typename T>
class ObjectPool { };

template <typename Impl, typename... Args>
static std::shared_ptr<Logger> DefaultWithArgs(Args&&... args);

// Bad
template <typename data, typename error>
class Result { };

template <typename t>
class ObjectPool { };
```

---

## File Organization and Structure

### File Naming

- **Header files**: Use `.hpp` for C++ headers (pure C++ code), `.h` for C-compatible or Windows-specific headers
- **Implementation files**: Use `.cpp` for all implementation files
- **File names**: Match the primary class name using PascalCase

```
// Good
NetworkTransport.hpp / NetworkTransport.cpp
IocpAcceptor.h / IocpAcceptor.cpp
Client.h / Client.cpp
Result.hpp

// Bad
network_transport.hpp
iocpacceptor.h
client.cpp
result.h
```

### Directory Structure

Organize code by architectural layer:

```
network/          # Network layer - IOCP primitives, sockets
lib/              # Shared utilities - Result, Logger, etc.
  compileTime/    # Compile-time utilities (TOML parsing)
  runTime/        # Runtime utilities
exec/             # Executable projects
  echo/
    echo-server/  # Server application
    echo-client/  # Client application
docs/             # Documentation
scripts/          # Build and configuration scripts
```

### File Structure Template

```cpp
// 1. Header guard or #pragma once
#pragma once

// 2. System includes
#include <memory>
#include <string_view>

// 3. Project includes (relative to project structure)
#include "platform.h"
#include <Result.hpp>
#include <Logger.hpp>

// 4. Namespace declaration
namespace highp::network {

// 5. Forward declarations (if needed)
class SocketOptionBuilder;

// 6. Type aliases and using declarations
using AcceptCallback = std::function<void(AcceptContext&)>;

// 7. Class/struct declarations with documentation
/// <summary>
/// Brief description of the class.
/// </summary>
class MyClass {
public:
    // Public interface

private:
    // Private implementation
};

} // namespace highp::network
```

### Implementation File Structure

```cpp
// 1. PCH include (if using precompiled headers)
#include "pch.h"

// 2. Corresponding header
#include "MyClass.h"

// 3. Additional includes
#include <Errors.hpp>

// 4. Namespace
namespace highp::network {

// 5. Implementation
MyClass::MyClass() {
    // Constructor
}

// Method implementations...

} // namespace highp::network
```

---

## Header File Best Practices

### Include Guards

Use `#pragma once` instead of traditional include guards:

```cpp
// Good
#pragma once

// Bad
#ifndef NETWORK_TRANSPORT_HPP
#define NETWORK_TRANSPORT_HPP
// ...
#endif
```

### Include Order

1. Precompiled header (pch.h) - only in .cpp files
2. Corresponding header (for .cpp files)
3. Project headers
4. Standard library headers

```cpp
// In IocpAcceptor.cpp
#include "pch.h"
#include "IocpAcceptor.h"
#include <Errors.hpp>
#include <memory>
```

### Forward Declarations

Use forward declarations in headers to reduce compile-time dependencies:

```cpp
// Good - IocpAcceptor.h
namespace highp::network {
    class SocketOptionBuilder;  // Forward declaration

    class IocpAcceptor {
    private:
        std::shared_ptr<SocketOptionBuilder> _socketOptionBuilder;
    };
}

// Bad - Including full header when forward declaration suffices
#include "SocketOptionBuilder.h"
```

### Header-Only Utilities

Place template implementations and `constexpr` functions in headers:

```cpp
// Good - Result.hpp (header-only template class)
namespace highp::fn {

template <typename TData, typename E>
class [[nodiscard]] Result final {
    // Full implementation in header
};

} // namespace highp::fn
```

### Platform-Specific Headers

Use conditional compilation for platform-specific code:

```cpp
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#elif _LINUX
#include <sys/socket.h>
#endif
```

---

## Comment and Documentation Style

### XML Documentation Comments

Use C# style XML documentation comments (`/// <summary>`) for all public APIs:

```cpp
/// <summary>
/// AcceptEx 기반 비동기 연결 수락을 담당하는 클래스.
/// WSAIoctl로 AcceptEx 함수 포인터를 획득하여 간접 호출 방식을 사용한다.
/// </summary>
/// <remarks>
/// 사용 순서:
/// 1. Initialize()로 Listen 소켓을 IOCP에 연결하고 AcceptEx 함수 로드
/// 2. SetAcceptCallback()으로 연결 수락 콜백 등록
/// 3. PostAccepts()로 비동기 Accept 요청
/// </remarks>
class IocpAcceptor final {
    /// <summary>
    /// IocpAcceptor를 초기화한다.
    /// </summary>
    /// <param name="listenSocket">Listen 상태의 소켓 핸들</param>
    /// <param name="iocpHandle">IOCP 핸들</param>
    /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
    Res Initialize(SocketHandle listenSocket, HANDLE iocpHandle);
};
```

### Inline Comments

Use inline comments for complex logic or non-obvious code:

```cpp
// Listen 소켓을 IOCP에 연결 (AcceptEx 완료 통지를 받기 위해 필수)
HANDLE result = CreateIoCompletionPort(
    reinterpret_cast<HANDLE>(_listenSocket),
    _iocpHandle,
    0,  // Listen 소켓은 completionKey 불필요
    0);
```

### TODO Comments

Use structured TODO comments with context:

```cpp
// TODO: #1 logger options 추가.
// TODO: #2 logger port (console, file, OTEL ...) 추가
// TODO: #3 logger format (text, json, ...) 추가
```

### Critical Invariants

Document critical invariants and requirements prominently:

```cpp
/// <remarks>
/// WSAOVERLAPPED가 반드시 첫 번째 멤버여야 하는 이유:
/// - WSARecv, WSASend, AcceptEx 등에 LPOVERLAPPED로 전달
/// - GetQueuedCompletionStatus()에서 반환된 LPOVERLAPPED를 OverlappedExt*로 캐스팅
/// - 첫 번째 멤버가 아니면 메모리 레이아웃 불일치로 잘못된 주소 참조 발생
/// </remarks>
struct SendOverlapped {
    WSAOVERLAPPED overlapped;  // Must be first member!
    // ...
};
```

### Code Evolution Comments

Document code evolution and alternatives considered:

```cpp
// 0) 원본 C++17 if with initializer 로 2줄 -> 1줄로 줄이기!
//if (auto res = LoadAcceptExFunctions(); res.HasErr()) {
//    return res;
//}

// 1) bool 연산자 오버로딩 -> .HasErr() 를 줄일 수 있음.
//if (auto res = LoadAcceptExFunctions(); !res) {
//    return res;
//}

// 2) explicit result check
if (auto res = LoadAcceptExFunctions(); res.HasErr()) {
    return res;
}
```

---

## Code Formatting Guidelines

### Indentation and Spacing

- **Indentation**: Use tabs (as seen in existing code)
- **Braces**: Opening brace on same line for functions, control structures
- **Spacing**: Single space after control keywords, around operators

```cpp
// Good
if (result == NULL || result != _iocpHandle) {
    _logger->Error("Failed to associate listen socket with IOCP. error: {}", GetLastError());
    return Res::Err(err::ENetworkError::SocketCreateFailed);
}

for (int i = 0; i < count; ++i) {
    if (auto res = PostAccept(); res.HasErr()) {
        return res;
    }
}

// Bad
if(result==NULL||result!=_iocpHandle)
{
    _logger->Error("Failed");
    return Res::Err(err::ENetworkError::SocketCreateFailed);
}
```

### Line Length

Keep lines reasonably short. Break long function calls at parameter boundaries:

```cpp
// Good
HANDLE result = CreateIoCompletionPort(
    reinterpret_cast<HANDLE>(_listenSocket),
    _iocpHandle,
    0,
    0);

// Acceptable for short calls
auto ptr = std::make_shared<T>();
```

### Member Initialization

Use member initializer lists for constructors:

```cpp
// Good
IocpAcceptor::IocpAcceptor(
    std::shared_ptr<log::Logger> logger,
    int preAllocCount,
    AcceptCallback onAfterAccept)
    : _logger(std::move(logger))
    , _overlappedPool(preAllocCount)
    , _acceptCallback(std::move(onAfterAccept))
{
}

// Bad
IocpAcceptor::IocpAcceptor(std::shared_ptr<log::Logger> logger) {
    _logger = std::move(logger);
}
```

### Struct Initialization

Use designated initializers for clarity:

```cpp
// Good
_sockaddr = {
    .sin_family = AF_INET,
    .sin_port = htons(port)
};

CompileTimeTomlEntry{
    .section = currentSection,
    .key = key,
    .value = value,
    .ioType = ioType
};

// Acceptable for simple structs
ZeroMemory(&overlapped->overlapped, sizeof(WSAOVERLAPPED));
```

### Auto Usage

Use `auto` when the type is obvious from context or verbose:

```cpp
// Good
auto [socketType, protocol] = transport.GetInfos();
auto workerCount = std::thread::hardware_concurrency();
auto ptr = std::make_shared<Client>();

// Bad - type not obvious
auto result = ProcessData();  // What type is this?

// Good - explicit when clarity needed
SocketHandle acceptSocket = CreateAcceptSocket();
```

---

## Memory Management Patterns

### Smart Pointers

Use `std::shared_ptr` for shared ownership, `std::unique_ptr` for exclusive ownership:

```cpp
// Shared ownership - multiple components need access
class Logger {
    std::unique_ptr<ILogger> _impl;  // Logger exclusively owns implementation
public:
    template <typename Impl>
    static std::shared_ptr<Logger> Default() {
        return std::make_shared<Logger>(std::make_unique<Impl>());
    }
};

// Shared ownership across components
class IocpAcceptor {
    std::shared_ptr<log::Logger> _logger;  // Logger is shared
    std::shared_ptr<SocketOptionBuilder> _socketOptionBuilder;
};
```

### Object Pooling

Use `ObjectPool<T>` for frequently allocated/deallocated objects:

```cpp
// Good - Pre-allocate overlapped structures
class IocpAcceptor {
    mem::ObjectPool<AcceptOverlapped> _overlappedPool;

    AcceptOverlapped* AcquireOverlapped() {
        return _overlappedPool.Acquire();
    }

    void ReleaseOverlapped(AcceptOverlapped* overlapped) {
        _overlappedPool.Release(overlapped);
    }
};

// Usage
OverlappedExt* overlapped = AcquireOverlapped();
// ... use overlapped ...
ReleaseOverlapped(overlapped);
```

### RAII Pattern

Follow RAII for resource management:

```cpp
// Good - Destructor cleans up resources
class IocpAcceptor {
public:
    ~IocpAcceptor() noexcept {
        Shutdown();
    }

    void Shutdown() {
        _overlappedPool.Clear();
        _fnAcceptEx = nullptr;
        _fnGetAcceptExSockAddrs = nullptr;
    }
};

// Good - Resource cleanup in destructor
ServerLifeCycle::~ServerLifeCycle() noexcept {
    Stop();
}
```

### Move Semantics

Use `std::move` to transfer ownership:

```cpp
// Good
IocpAcceptor::IocpAcceptor(
    std::shared_ptr<log::Logger> logger,
    int preAllocCount,
    AcceptCallback onAfterAccept)
    : _logger(std::move(logger))  // Transfer ownership
    , _overlappedPool(preAllocCount)
    , _acceptCallback(std::move(onAfterAccept))
{
}

void SetAcceptCallback(AcceptCallback callback) {
    _acceptCallback = std::move(callback);
}
```

### Raw Pointers

Use raw pointers only for non-owning references from object pools:

```cpp
// Acceptable - Pool maintains ownership via shared_ptr
T* Acquire() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_available.empty()) {
        auto ptr = std::make_shared<T>();
        T* raw = ptr.get();
        _all.push_back(std::move(ptr));  // Pool owns via shared_ptr
        return raw;  // Return non-owning pointer
    }
    // ...
}
```

### Memory Initialization

Use `ZeroMemory` for Windows structures, value initialization for C++ types:

```cpp
// Good - Windows structures
ZeroMemory(&overlapped->overlapped, sizeof(WSAOVERLAPPED));
ZeroMemory(&recvOverlapped, sizeof(RecvOverlapped));

// Good - C++ initialization
SocketHandle socket = INVALID_SOCKET;
char clientIp[Const::Buffer::clientIpBufferSize]{ 0 };
```

---

## Error Handling Style

### Result<T, E> Type

Use `Result<T, E>` for operations that can fail:

```cpp
// Type alias for convenience
class IocpAcceptor {
public:
    using Res = fn::Result<void, err::ENetworkError>;

    Res Initialize(SocketHandle listenSocket, HANDLE iocpHandle);
    Res PostAccept();
    Res PostAccepts(int count);
};

// Return Ok or Err
Res IocpAcceptor::PostAccept() {
    SocketHandle acceptSocket = CreateAcceptSocket();
    if (acceptSocket == InvalidSocket) {
        _logger->Error("Failed to create accept socket. error: {}", WSAGetLastError());
        return Res::Err(err::ENetworkError::SocketCreateFailed);
    }

    // ... success case ...
    return Res::Ok();
}
```

### Explicit Result Checks
Use explicit if statements for error propagation:
`cpp
if (auto res = LoadAcceptExFunctions(); res.HasErr()) {
    return res;
}
if (auto res = PostAccept(); res.HasErr()) {
    _logger->Error("Failed to re-post AcceptEx after completion.");
    return res;
}
`
Prefer small named temporaries and straightforward control flow over helper macros.
### Result Checking

Check results explicitly:

```cpp
// Good - Explicit propagation
Res Start() {
    if (auto res = Initialize(); res.HasErr()) {
        return res;
    }
    if (auto res = PostAccepts(10); res.HasErr()) {
        return res;
    }
    return Res::Ok();
}

// Good - Explicit check with custom handling
Res Start() {
    if (auto res = Initialize(); res.HasErr()) {
        _logger->Error("Initialization failed");
        Cleanup();
        return res;
    }
    return Res::Ok();
}

// Good - Using bool operator
if (auto res = Initialize(); !res) {
    return res;
}

// Bad - Ignoring result (will cause compiler warning due to [[nodiscard]])
Initialize();  // Warning!
```

### Error Logging Helpers

Use templated error logging helpers:

```cpp
// Log error and return Result
return err::LogErrorWSAWithResult<err::ENetworkError::SocketCreateFailed>(_logger);

return err::LogErrorWindowsWithResult<err::ENetworkError::IocpCreateFailed>(_logger);

// Just log without returning
err::LogErrorWSA<err::ENetworkError::SocketBindFailed>(_logger);
```

### Error Code Enums

Define error codes as enum classes with ToString functions:

```cpp
enum class ENetworkError {
    SocketCreateFailed,
    SocketBindFailed,
    SocketListenFailed,
    WsaStartupFailed,
    // ...
};

// Provide ToString conversion
constexpr std::string_view ToString(ENetworkError err) {
    switch (err) {
        case ENetworkError::SocketCreateFailed: return "Socket creation failed";
        // ...
    }
}
```

### Windows API Error Checking

Check for specific error codes when appropriate:

```cpp
int result = WSARecv(
    socket,
    &recvOverlapped.bufDesc,
    1,
    &recvNumBytes,
    &flags,
    reinterpret_cast<LPWSAOVERLAPPED>(&recvOverlapped),
    nullptr);

if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
    return Res::Err(err::ENetworkError::WsaRecvFailed);
}
```

---

## Threading and Concurrency Patterns

### Thread Safety with Mutex

Use `std::mutex` with RAII locking for thread-safe operations:

```cpp
template <typename T>
class ObjectPool {
private:
    std::mutex _mutex;
    std::vector<std::shared_ptr<T>> _all;
    std::vector<T*> _available;

public:
    T* Acquire() {
        std::lock_guard<std::mutex> lock(_mutex);  // RAII lock
        // ... critical section ...
    }

    void Release(T* obj) {
        if (!obj) return;
        std::lock_guard<std::mutex> lock(_mutex);
        _available.push_back(obj);
    }
};
```

### Atomic Operations

Use `std::atomic` for simple counters:

```cpp
class ServerLifeCycle {
private:
    std::atomic<size_t> _connectedClientCount{ 0 };

public:
    size_t GetConnectedClientCount() const noexcept {
        return _connectedClientCount.load();
    }

    void OnAcceptInternal(AcceptContext& ctx) {
        // ...
        _connectedClientCount++;  // Atomic increment
    }
};
```

### Thread Management

Use `std::jthread` for automatic cleanup (when available):

```cpp
// Prefer jthread for automatic joining
std::vector<std::jthread> _workers;

// Or manage thread lifecycle explicitly
void Start() {
    for (int i = 0; i < workerCount; ++i) {
        _workers.emplace_back(&Class::WorkerLoop, this);
    }
}

void Stop() {
    _stopFlag = true;
    // Join threads...
}
```

### Callbacks and Function Binding

Use `std::function` for callbacks, `std::bind_front` for member function binding:

```cpp
using AcceptCallback = std::function<void(AcceptContext&)>;

// Bind member function as callback
_iocp = std::make_unique<IocpIoMultiplexer>(
    _logger,
    std::bind_front(&ServerLifeCycle::OnCompletion, this));

_acceptor = std::make_unique<IocpAcceptor>(
    _logger,
    _config.server.backlog,
    std::bind_front(&ServerLifeCycle::OnAcceptInternal, this));
```

### Thread-Safe Initialization

Ensure resources are initialized before multi-threaded access:

```cpp
Res Start() {
    // Initialize IOCP
    _iocp = std::make_unique<IocpIoMultiplexer>(...);
    if (auto res = _iocp->Initialize(workerCount); res.HasErr()) { return res; }  // Starts worker threads

    // Initialize acceptor
    _acceptor = std::make_unique<IocpAcceptor>(...);
    if (auto res = _acceptor->Initialize(...); res.HasErr()) { return res; }

    // Pre-allocate client pool before workers start processing
    _clientPool.reserve(_config.server.maxClients);
    for (int i = 0; i < _config.server.maxClients; ++i) {
        _clientPool.emplace_back(std::make_shared<Client>());
    }

    return Res::Ok();
}
```

---

## Platform-Specific Code Guidelines

### Conditional Compilation

Use preprocessor directives for platform-specific code:

```cpp
#ifdef _WIN32
/// <summary>Windows 플랫폼용 네트워크 전송 프로토콜 래퍼.</summary>
class WindowsNetworkTransport {
    // Windows implementation
};
#elif _LINUX
/// <summary>Linux 플랫폼용 네트워크 전송 프로토콜 래퍼. (미구현)</summary>
class LinuxNetworkTransport {
    // Linux implementation
};
#endif
```

### Type Aliases for Platform Abstraction

Use type aliases to select platform-specific implementations:

```cpp
#ifdef _WIN32
using NetworkTransport = WindowsNetworkTransport;
#elif _LINUX
using NetworkTransport = LinuxNetworkTransport;
#endif
```

### Platform-Specific Headers

Organize platform headers in `platform.h`:

```cpp
// platform.h
#pragma once

#ifdef _WIN32
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #include <MSWSock.h>
    using SocketHandle = SOCKET;
    constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#elif _LINUX
    #include <sys/socket.h>
    using SocketHandle = int;
    constexpr SocketHandle InvalidSocket = -1;
#endif
```

### Consistent Interface

Ensure platform-specific classes provide consistent interfaces:

```cpp
class WindowsNetworkTransport {
public:
    explicit WindowsNetworkTransport(ETransport transportType);
    [[nodiscard]] constexpr std::pair<INT, IPPROTO> GetInfos() const;
    ETransport GetTransportType() const;
};

class LinuxNetworkTransport {
public:
    explicit LinuxNetworkTransport(ETransport transportType);
    [[nodiscard]] constexpr std::pair<INT, IPPROTO> GetInfos() const;
    ETransport GetTransportType() const;
};
```

---

## Examples of Good vs Bad Style

### Example 1: Error Handling

```cpp
// Bad - Ignoring error results
void Initialize() {
    CreateSocket();  // What if this fails?
    Bind(8080);      // Continues even if CreateSocket failed
}

// Good - Proper error propagation
Res Initialize() {
    if (auto res = CreateSocket(); res.HasErr()) { return res; }
    if (auto res = Bind(8080); res.HasErr()) { return res; }
    return Res::Ok();
}
```

### Example 2: Resource Management

```cpp
// Bad - Manual memory management
class BadAcceptor {
    OverlappedExt* _overlapped;
public:
    BadAcceptor() {
        _overlapped = new OverlappedExt[100];
    }
    ~BadAcceptor() {
        delete[] _overlapped;  // Easy to forget or leak
    }
};

// Good - RAII with object pool
class IocpAcceptor {
    mem::ObjectPool<AcceptOverlapped> _overlappedPool;
public:
    explicit IocpAcceptor(int preAllocCount)
        : _overlappedPool(preAllocCount) {
        // Automatic cleanup via ObjectPool destructor
    }
};
```

### Example 3: Initialization

```cpp
// Bad - Assignment in constructor body
Client::Client() {
    socket = INVALID_SOCKET;
    ZeroMemory(&recvOverlapped, sizeof(RecvOverlapped));
}

// Good - Member initializer list + explicit initialization
Client::Client()
    : socket(INVALID_SOCKET) {
    ZeroMemory(&recvOverlapped, sizeof(RecvOverlapped));
    ZeroMemory(&sendOverlapped, sizeof(SendOverlapped));
}
```

### Example 4: Naming Consistency

```cpp
// Bad - Inconsistent naming
class bad_acceptor {
    shared_ptr<Logger> m_Logger;
    HANDLE IOCPHandle;

    void post_accept();
    void OnAcceptComplete();
};

// Good - Consistent naming
class IocpAcceptor {
    std::shared_ptr<log::Logger> _logger;
    HANDLE _iocpHandle;

    Res PostAccept();
    Res OnAcceptComplete(AcceptOverlapped* overlapped, DWORD bytesTransferred);
};
```

### Example 5: Documentation

```cpp
// Bad - No documentation
class Client {
    Res PostRecv();
};

// Good - Clear documentation
/// <summary>
/// 클라이언트 연결 상태 및 I/O 버퍼를 관리하는 구조체.
/// std::enable_shared_from_this를 상속하여 콜백에서 안전하게 shared_ptr 획득 가능.
/// </summary>
struct Client : public std::enable_shared_from_this<Client> {
    /// <summary>
    /// 비동기 수신을 시작한다.
    /// </summary>
    /// <returns>성공 시 Ok, 실패 시 에러 코드</returns>
    Res PostRecv();
};
```

### Example 6: Modern C++ Features

```cpp
// Bad - Old-style code
Result<void, ENetworkError> LoadFunctions() {
    Result<void, ENetworkError> res = LoadAcceptExFunctions();
    if (res.HasErr() == true) {
        return res;
    }
    return Result<void, ENetworkError>::Ok();
}

// Good - Modern C++ (if-init, auto, explicit checks)
Res LoadFunctions() {
    if (auto res = LoadAcceptExFunctions(); res.HasErr()) {
        return res;
    }
    return Res::Ok();
}

// Alternative - if with initializer
if (auto res = LoadAcceptExFunctions(); res.HasErr()) {
    return res;
}
```

### Example 7: Const Correctness

```cpp
// Bad - Missing const
class ObjectPool {
    size_t AvailableCount() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _available.size();
    }
};

// Good - Const member function
class ObjectPool {
    size_t AvailableCount() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _available.size();
    }
};
```

### Example 8: Template Usage

```cpp
// Bad - Not generic enough
class IntResult {
    int _value;
    bool _hasError;
};

class StringResult {
    std::string _value;
    bool _hasError;
};

// Good - Generic template
template <typename TData, typename E>
class [[nodiscard]] Result final {
    TData _data;
    E _err;
    bool _hasError;

    static Result Ok(TData data) { return Result(std::move(data)); }
    static Result Err(E err) { return Result(err, true); }
};
```

### Example 9: Switch Statements

```cpp
// Bad - Missing default case
void OnCompletion(CompletionEvent event) {
    switch (event.ioType) {
        case EIoType::Accept:
            HandleAccept(event);
            break;
        case EIoType::Recv:
            HandleRecv(event);
            break;
    }
}

// Good - Explicit default case
void OnCompletion(CompletionEvent event) {
    switch (event.ioType) {
        case EIoType::Accept:
            auto* overlapped = reinterpret_cast<AcceptOverlapped*>(event.overlapped);
            if (_acceptor) {
                _acceptor->OnAcceptComplete(overlapped, event.bytesTransferred);
            }
            break;

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

### Example 10: Lambda Usage

```cpp
// Good - Lambda for simple predicates
std::shared_ptr<Client> FindAvailableClient() {
    auto found = std::find_if(_clientPool.begin(), _clientPool.end(),
        [](const std::shared_ptr<Client>& c) {
            return c->socket == INVALID_SOCKET;
        });
    if (found != _clientPool.end()) {
        return *found;
    }
    return nullptr;
}

// Good - Lambda-equivalent side effect before return
if (auto res = PostAccept(); res.HasErr()) {
    _logger->Error("Failed to re-post AcceptEx after completion.");
    return res;
}
```

---

## Summary Checklist

When writing new code, ensure:

- [ ] PascalCase for types, classes, methods
- [ ] camelCase for parameters, local variables, and private members (with `_` prefix)
- [ ] snake_case for TOML configuration keys
- [ ] XML documentation (`/// <summary>`) for public APIs
- [ ] `#pragma once` for header guards
- [ ] `Result<T, E>` for error handling with explicit checks
- [ ] Smart pointers (`shared_ptr`, `unique_ptr`) for ownership
- [ ] RAII for resource management
- [ ] `std::lock_guard` for thread safety
- [ ] Platform abstractions with `#ifdef` and type aliases
- [ ] Const correctness and `[[nodiscard]]` attributes
- [ ] Designated initializers for struct initialization
- [ ] Move semantics with `std::move`
- [ ] Explicit error checking and logging
- [ ] Consistent file organization and naming

---

## References

- CLAUDE.md - Project overview and architecture
- network/ - Network layer implementation examples
- lib/ - Utility library implementation examples
- Result.hpp - Error handling pattern implementation
- ObjectPool.hpp - Memory management pattern implementation

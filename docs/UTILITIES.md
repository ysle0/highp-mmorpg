# Utilities Reference

Comprehensive guide to the utility libraries in `lib/` that provide foundational infrastructure for the highp-mmorpg project.

## Table of Contents

1. [Overview](#overview)
2. [Logger System](#logger-system)
3. [Result Type](#result-type)
4. [Object Pool](#object-pool)
5. [TOML Configuration](#toml-configuration)
6. [String Utilities](#string-utilities)
7. [Error Handling](#error-handling)
8. [Platform Abstractions](#platform-abstractions)
9. [Performance Considerations](#performance-considerations)
10. [Best Practices](#best-practices)

## Overview

The `lib/` directory contains reusable, general-purpose utilities designed with the following principles:

- **Zero-cost abstractions**: Templates and constexpr where possible
- **Type safety**: Strong typing with `Result<T, E>` instead of error codes
- **Compile-time optimization**: `constexpr` and `consteval` for configuration parsing
- **Minimal dependencies**: Standard library only, no external dependencies
- **Thread safety**: Explicit synchronization where needed (e.g., `ObjectPool`)

### Design Philosophy

1. **Separation of concerns**: Each utility has a single, well-defined purpose
2. **Composability**: Utilities work together (e.g., `Logger` + `Result` + `Errors`)
3. **RAII**: Resource management through constructors/destructors
4. **No exceptions**: Use `Result<T, E>` for error propagation
5. **Explicit over implicit**: Avoid hidden control flow or magic behavior

## Logger System

The logging system provides a flexible, polymorphic interface for structured output across different backends.

### Architecture

```
Logger (facade)
  └─> ILogger (interface)
       ├─> TextLogger (console output)
       ├─> StructuredLogger (future: JSON/OTEL)
       └─> FileLogger (future: file output)
```

### Core Components

#### `ILogger` Interface (`lib/ILogger.h`)

Abstract interface defining logging contract:

```cpp
namespace highp::log {
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void Info(std::string_view) PURE;
    virtual void Debug(std::string_view) PURE;
    virtual void Warn(std::string_view) PURE;
    virtual void Error(std::string_view) PURE;
    virtual void Exception(std::string_view, std::exception const&) PURE;
};
}
```

**Design notes:**
- Pure virtual interface allows backend swapping
- `PURE` macro expands to `= 0` (see `lib/macro.h`)
- `std::string_view` avoids unnecessary string copies
- No formatting logic in interface (delegated to implementations)

#### `Logger` Facade (`lib/Logger.hpp`)

Wrapper providing `std::format` support and unified API:

```cpp
namespace highp::log {
class Logger {
    std::unique_ptr<ILogger> _impl;
public:
    explicit Logger(std::unique_ptr<ILogger> impl);

    // Factory methods
    template <typename Impl>
    static std::shared_ptr<Logger> Default();

    template <typename Impl, typename... Args>
    static std::shared_ptr<Logger> DefaultWithArgs(Args&&... args);

    // Simple string logging
    void Info(std::string_view msg);
    void Debug(std::string_view msg);
    void Warn(std::string_view msg);
    void Error(std::string_view msg);
    void Exception(std::string_view msg, std::exception const& ex);

    // std::format-based logging
    template<class... Args>
    void Info(std::format_string<Args...> fmt, Args&&... args);
    // ... (Debug, Warn, Error variants)
};
}
```

**Key features:**
- Type-erased implementation storage via `std::unique_ptr<ILogger>`
- Factory methods for convenient construction
- Template overloads for `std::format` integration
- Delegation pattern: formatting happens in `Logger`, output in `ILogger` impl

#### `TextLogger` Implementation (`lib/TextLogger.h`, `lib/TextLogger.cpp`)

Console output implementation:

```cpp
namespace highp::log {
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
}
```

**Implementation** (excerpts from `TextLogger.cpp`):

```cpp
void TextLogger::Info(std::string_view msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

void TextLogger::Error(std::string_view msg) {
    std::cout << "[ERROR] " << msg << std::endl;
}

void TextLogger::Exception(std::string_view msg, std::exception const& ex) {
    std::cout << "[EXCEPTION] " << msg << ": " << ex.what() << std::endl;
}
```

#### `ELoggerVerbosity` Enum (`lib/ELoggerVerbosity.h`)

Verbosity levels for filtering (currently unused, planned for future):

```cpp
namespace highp::log {
enum class ELoggerVerbosity {
    Info = 10,
    Debug = 20,
    Error = 30,
    Exception = 40
};
}
```

### Usage Examples

#### Basic Usage

```cpp
#include <Logger.hpp>
#include <TextLogger.h>

using namespace highp::log;

int main() {
    // Create logger with default TextLogger
    auto logger = Logger::Default<TextLogger>();

    // Simple logging
    logger->Info("Server starting...");
    logger->Debug("Initializing IOCP");
    logger->Warn("High memory usage detected");
    logger->Error("Failed to bind socket");

    // With std::format
    logger->Info("Listening on port {}", 8080);
    logger->Debug("Worker threads: {}, Max clients: {}", 4, 100);

    // Exception logging
    try {
        throw std::runtime_error("Database connection failed");
    } catch (const std::exception& ex) {
        logger->Exception("Critical error", ex);
    }
}
```

#### Dependency Injection Pattern

```cpp
class EchoServer {
    std::shared_ptr<log::Logger> _logger;
public:
    explicit EchoServer(std::shared_ptr<log::Logger> logger)
        : _logger(logger) {}

    void Start() {
        _logger->Info("Starting EchoServer");
        // ...
    }
};

int main() {
    auto logger = Logger::Default<TextLogger>();
    EchoServer server(logger);
    server.Start();
}
```

#### Advanced: Custom Logger Backend

```cpp
class FileLogger : public ILogger {
    std::ofstream _file;
public:
    explicit FileLogger(const std::string& filename)
        : _file(filename, std::ios::app) {}

    void Info(std::string_view msg) override {
        _file << "[INFO] " << msg << std::endl;
    }
    // ... other methods
};

// Usage
auto logger = Logger::DefaultWithArgs<FileLogger>("server.log");
```

### Future Enhancements (TODOs in `Logger.hpp`)

- **Logger options**: Verbosity filtering, timestamp formatting
- **Additional ports**: File output, OpenTelemetry (OTEL), network logging
- **Format support**: JSON, structured logs for machine parsing

## Result Type

`Result<T, E>` provides Rust-style error handling without exceptions. Similar to C++23 `std::expected`.

### Type Definition (`lib/Result.hpp`)

```cpp
namespace highp::fn {

template <typename TData, typename E>
class [[nodiscard]] Result final {
private:
    TData _data;
    E _err;
    bool _hasError;

public:
    // Factory methods
    static Result Ok();                  // void success
    static Result Ok(TData data);        // success with data
    static Result Err(E err);            // failure

    // Inspection
    bool IsOk() const;
    bool HasErr() const;
    TData Data() const;
    E Err() const;

    explicit operator bool() const;      // IsOk()
};

// Void specialization
template <typename E>
class [[nodiscard]] Result<void, E> final {
private:
    E _err;
    bool _hasError;

public:
    static Result Ok();
    static Result Err(E err);

    bool IsOk() const;
    bool HasErr() const;
    E Err() const;

    explicit operator bool() const;
};

} // namespace highp::fn
```

**Design notes:**
- `[[nodiscard]]` forces callers to check results (compile-time safety)
- Two-state storage: `_hasError` flag + union-like `_data`/`_err`
- Explicit factories (`Ok`, `Err`) prevent accidental construction
- `void` specialization avoids dummy return values

### Explicit Result Check Patterns
Use explicit Result checks for early exit instead of helper macros:
`cpp
// Return error if result has error
if (const auto result = expr; result.HasErr()) {
    return result;
}
// Return void if result has error
if (const auto result = expr; result.HasErr()) {
    return;
}
// Break loop if result has error
if (const auto result = expr; result.HasErr()) {
    break;
}
// Execute effect function before returning error
if (const auto result = expr; result.HasErr()) {
    fn();
    return result;
}
`
### Usage Examples
#### Basic Result Handling
`cpp
#include <Result.hpp>
#include <NetworkError.h>
using namespace highp::fn;
using namespace highp::err;
// Function returning result with data
Result<int, ENetworkError> ConnectToServer(const char* host) {
    int socket = CreateSocket();
    if (socket == INVALID_SOCKET) {
        return Result<int, ENetworkError>::Err(ENetworkError::SocketCreateFailed);
    }
    return Result<int, ENetworkError>::Ok(socket);
}
// Function returning void result
Result<void, ENetworkError> BindSocket(int socket, int port) {
    if (bind(socket, ...) != 0) {
        return Result<void, ENetworkError>::Err(ENetworkError::SocketBindFailed);
    }
    return Result<void, ENetworkError>::Ok();
}
// Checking results
void Example1() {
    auto result = ConnectToServer("127.0.0.1");
    if (result.IsOk()) {
        int socket = result.Data();
        std::cout << "Connected: socket=" << socket << std::endl;
    } else {
        std::cerr << "Connection failed: " << ToString(result.Err()) << std::endl;
    }
}
`
#### Explicit Early Return
`cpp
Result<void, ENetworkError> InitializeNetwork() {
    if (const auto wsaResult = WSAStartup(...); wsaResult.HasErr()) {
        return wsaResult;
    }
    auto socketResult = ConnectToServer("127.0.0.1");
    if (socketResult.HasErr()) {
        return socketResult;
    }
    int socket = socketResult.Data();
    if (const auto bindResult = BindSocket(socket, 8080); bindResult.HasErr()) {
        return bindResult;
    }
    return Result<void, ENetworkError>::Ok();
}
`
#### Explicit Cleanup Before Return
`cpp
Result<void, ENetworkError> PostAccepts(int count) {
    for (int i = 0; i < count; ++i) {
        if (const auto postAcceptResult = PostAccept(); postAcceptResult.HasErr()) {
            _logger->Error("Failed to post accept #{}", i);
            return postAcceptResult;
        }
    }
    return Result<void, ENetworkError>::Ok();
}
`
#### Practical Example from Codebase
From 
etwork/IocpAcceptor.cpp:
`cpp
Result<void, ENetworkError> IocpAcceptor::Initialize(
    SocketHandle listenSocket,
    HANDLE iocpHandle) {
    _listenSocket = listenSocket;
    _iocpHandle = iocpHandle;
    if (const auto iocpResult = ConnectToIocp(); iocpResult.HasErr()) {
        return iocpResult;
    }
    if (const auto loadAcceptExResult = LoadAcceptExFunctions(); loadAcceptExResult.HasErr()) {
        return loadAcceptExResult;
    }
    return Result<void, ENetworkError>::Ok();
}
`
### Error Type Guidelines

- Use strongly-typed enums (e.g., `ENetworkError`, `EConfigError`)
- Provide `ToString()` function for each error enum
- Store error enums in `lib/` for reusability (e.g., `lib/NetworkError.h`)
- Use specific error types per domain (network, file I/O, etc.)

## Object Pool

`ObjectPool<T>` provides memory pooling for frequently allocated/deallocated objects. Reduces allocation overhead and improves cache locality.

### Type Definition (`lib/ObjectPool.hpp`)

```cpp
namespace highp::mem {

template <typename T>
class ObjectPool final {
public:
    explicit ObjectPool(int preAllocCount = 0);
    ~ObjectPool() = default;

    // Non-copyable, non-movable
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

    void PreAllocate(int count);
    T* Acquire();
    void Release(T* obj);
    void Clear();

    size_t AvailableCount();
    size_t TotalCount();

private:
    std::vector<std::shared_ptr<T>> _all;        // Owns all objects
    std::vector<T*> _available;                  // Tracks free objects
    std::mutex _mutex;                           // Thread-safe access
};

} // namespace highp::mem
```

### Implementation Details

**Two-container design:**
- `_all`: Owns all allocated objects via `std::shared_ptr<T>` (never shrinks)
- `_available`: Raw pointers to objects available for reuse

**Thread safety:**
- All public methods protected by `std::lock_guard<std::mutex>`
- Safe for concurrent `Acquire()` and `Release()` calls

**Allocation strategy:**
- `Acquire()`: Pop from `_available`, or allocate new if empty
- `Release()`: Push back to `_available` (no deallocation)
- `PreAllocate()`: Create objects upfront for predictable memory usage

### Usage Examples

#### Basic Usage

```cpp
#include <ObjectPool.hpp>

using namespace highp::mem;

struct NetworkBuffer {
    char data[4096];
    size_t size = 0;

    void Reset() { size = 0; }
};

int main() {
    // Pre-allocate 100 buffers
    ObjectPool<NetworkBuffer> bufferPool(100);

    // Acquire buffer
    NetworkBuffer* buf = bufferPool.Acquire();
    buf->size = 1024;
    // ... use buffer

    // Release back to pool
    buf->Reset();
    bufferPool.Release(buf);

    std::cout << "Available: " << bufferPool.AvailableCount() << std::endl;
    std::cout << "Total: " << bufferPool.TotalCount() << std::endl;
}
```

#### IOCP Overlapped Context Pooling

From `network/IocpAcceptor.h`:

```cpp
class IocpAcceptor final {
    mem::ObjectPool<OverlappedExt> _overlappedPool;

public:
    explicit IocpAcceptor(
        std::shared_ptr<log::Logger> logger,
        std::shared_ptr<SocketOptionBuilder> socketOptionBuilder = nullptr,
        int preAllocCount = 10)
        : _logger(logger)
        , _socketOptionBuilder(socketOptionBuilder)
        , _overlappedPool(preAllocCount)  // Pre-allocate 10 OVERLAPPED structs
    {}

    Result<void, ENetworkError> PostAccept() {
        // Acquire from pool instead of heap allocation
        OverlappedExt* overlapped = _overlappedPool.Acquire();
        overlapped->ioType = EIOType::Accept;
        overlapped->data = nullptr;
        overlapped->socket = CreateSocket();

        // ... use overlapped for AcceptEx

        // Release back to pool after I/O completes
        _overlappedPool.Release(overlapped);
        return Result<void, ENetworkError>::Ok();
    }
};
```

#### Dynamic Scaling

```cpp
ObjectPool<DatabaseConnection> connPool(10);  // Start with 10

void HandleRequestBurst() {
    std::vector<DatabaseConnection*> conns;

    // Pool auto-grows if needed
    for (int i = 0; i < 50; ++i) {
        conns.push_back(connPool.Acquire());  // May allocate new
    }

    // Return all
    for (auto* conn : conns) {
        connPool.Release(conn);
    }

    // Now pool has 50 objects for future use
}
```

### Performance Characteristics

- **Acquire**: O(1) amortized (pop from vector or single allocation)
- **Release**: O(1) (push to vector)
- **Memory**: Fixed after peak usage (pool never shrinks)
- **Thread contention**: Mutex can bottleneck under high concurrency

**When to use:**
- Objects allocated/deallocated frequently (e.g., I/O buffers, OVERLAPPED contexts)
- Object construction is expensive
- Predictable memory footprint is acceptable
- Peak concurrency is moderate (mutex contention is manageable)

**When NOT to use:**
- Infrequent allocations
- Highly variable object sizes
- Extremely high concurrency (consider lock-free alternatives)

## TOML Configuration

Dual configuration systems: **compile-time** (constexpr) and **runtime** (file-based). Both parse TOML-like format.

### Architecture Overview

```
Configuration
├─ Compile-Time (lib/compileTime/)
│   ├─ StringUtils.hpp        (constexpr string ops)
│   ├─ TomlEntry.hpp          (constexpr entry struct)
│   ├─ TomlParser.hpp         (consteval parser)
│   └─ Config.hpp             (constexpr accessor)
│
└─ Runtime (lib/runTime/)
    ├─ StringUtils.hpp        (runtime string ops)
    ├─ TomlEntry.hpp          (runtime entry struct)
    ├─ TomlParser.hpp         (runtime parser)
    └─ Config.hpp             (runtime accessor + env var support)
```

### Compile-Time Configuration

#### `CompileTimeConfig` (`lib/compileTime/Config.hpp`)

Parse TOML at compile-time for zero-runtime-cost constants:

```cpp
namespace highp::config {

template<size_t MaxEntries = 64>
class CompileTimeConfig {
public:
    static consteval CompileTimeConfig From(std::string_view tomlContent) noexcept;

    [[nodiscard]] constexpr std::string_view Str(
        std::string_view key,
        std::string_view defaultValue = {}) const noexcept;

    [[nodiscard]] constexpr int Int(
        std::string_view key,
        int defaultValue = 0) const noexcept;

    [[nodiscard]] constexpr long Long(
        std::string_view key,
        long defaultValue = 0) const noexcept;

    [[nodiscard]] constexpr bool Bool(
        std::string_view key,
        bool defaultValue = false) const noexcept;
};

// Convenience macro
#define DEFINE_CONSTEXPR_CONFIG(name, tomlContent) \
    inline constexpr auto name = ::highp::config::CompileTimeConfig<>::From(tomlContent)

} // namespace highp::config
```

#### Usage Example: Network Constants

```cpp
#include <compileTime/Config.hpp>

// Define config from embedded TOML string
DEFINE_CONSTEXPR_CONFIG(NetworkConst, R"(
[buffer]
recv_size = 4096
send_size = 1024

[limits]
max_clients = 1000
)");

// Use at compile-time
constexpr int RECV_BUFFER_SIZE = NetworkConst.Int("buffer.recv_size");
constexpr int SEND_BUFFER_SIZE = NetworkConst.Int("buffer.send_size");
constexpr int MAX_CLIENTS = NetworkConst.Int("limits.max_clients");

// These are truly constexpr - can be used in array sizes
char recvBuffer[RECV_BUFFER_SIZE];
std::array<Client, MAX_CLIENTS> clients;
```

#### Compile-Time String Utilities (`lib/compileTime/StringUtils.hpp`)

All operations are `constexpr` for compile-time execution:

```cpp
namespace highp::config {

struct CompileTimeStringUtils {
    static constexpr bool IsWhitespace(char c) noexcept;
    static constexpr bool IsDigit(char c) noexcept;
    static constexpr std::string_view Trim(std::string_view str) noexcept;
    static constexpr std::optional<size_t> Find(std::string_view str, char c) noexcept;
    static constexpr bool StartsWith(std::string_view str, std::string_view prefix) noexcept;
    static constexpr bool Equals(std::string_view a, std::string_view b) noexcept;
    static constexpr std::optional<int> ParseInt(std::string_view str) noexcept;
    static constexpr std::optional<long> ParseLong(std::string_view str) noexcept;
    static constexpr std::optional<bool> ParseBool(std::string_view str) noexcept;
    static constexpr std::string_view Unquote(std::string_view str) noexcept;
};

} // namespace highp::config
```

### Runtime Configuration

#### `Config` (`lib/runTime/Config.hpp`)

Load TOML from files with environment variable overrides:

```cpp
namespace highp::config {

class Config {
public:
    Config() = default;

    // Factory methods
    [[nodiscard]] static std::optional<Config> FromFile(const std::filesystem::path& path);
    [[nodiscard]] static Config FromString(std::string_view content);

    // Reload config
    bool Reload(const std::filesystem::path& path);

    // Accessors with env var override
    [[nodiscard]] std::string Str(
        std::string_view key,
        std::string_view defaultValue = {},
        const char* envVar = nullptr) const;

    [[nodiscard]] int Int(
        std::string_view key,
        int defaultValue = 0,
        const char* envVar = nullptr) const;

    [[nodiscard]] long Long(
        std::string_view key,
        long defaultValue = 0,
        const char* envVar = nullptr) const;

    [[nodiscard]] bool Bool(
        std::string_view key,
        bool defaultValue = false,
        const char* envVar = nullptr) const;

    [[nodiscard]] size_t EntryCount() const noexcept;
    [[nodiscard]] bool IsLoaded() const noexcept;
};

} // namespace highp::config
```

#### Usage Example: Server Configuration

From `network/NetworkCfg.h`:

```cpp
#include <runTime/Config.hpp>

class NetworkCfg {
    config::Config _config;
public:
    static NetworkCfg FromFile(const std::string& path) {
        auto config = config::Config::FromFile(path);
        return NetworkCfg(*config);
    }

    int GetPort() const {
        // Priority: SERVER_PORT env var > config file > default 8080
        return _config.Int("server.port", 8080, "SERVER_PORT");
    }

    int GetMaxClients() const {
        return _config.Int("server.max_clients", 100, "SERVER_MAX_CLIENTS");
    }

    int GetWorkerThreads() const {
        return _config.Int("server.worker_threads", 4, "SERVER_WORKER_THREADS");
    }
};

// Usage in main.cpp
int main() {
    auto config = NetworkCfg::FromFile("config.runtime.toml");
    EchoServer server(logger, config);
    server.Start();
}
```

#### Environment Variable Overrides

```bash
# Windows
set SERVER_PORT=9000
set SERVER_MAX_CLIENTS=500
echo-server.exe

# Linux
export SERVER_PORT=9000
export SERVER_MAX_CLIENTS=500
./echo-server
```

### TOML Format

Supports simplified TOML syntax:

```toml
# Comments start with #

[section_name]
string_key = "value"
int_key = 42
bool_key = true

[server]
host = "127.0.0.1"
port = 8080
max_clients = 1000
enable_logging = true
```

**Supported types:**
- String: `"quoted"` or `'quoted'`
- Integer: `123`, `-456`
- Boolean: `true`, `false`

**Limitations:**
- No arrays or tables (flat key-value only)
- No multiline strings
- No floats (use integers or strings)
- No nested sections (`[a.b.c]` not supported)

### Parser Implementation Details

Both compile-time and runtime parsers share the same logic:

1. **Line splitting**: Parse line-by-line, handle `\r\n` and `\n`
2. **Comment removal**: Strip `#` comments
3. **Section tracking**: Track current `[section]` for key-value pairs
4. **Key-value parsing**: Split on `=`, trim whitespace
5. **Type inference**: Detect string/int/bool from value format

**Compile-time parser** (`lib/compileTime/TomlParser.hpp`):
- `consteval` forces compile-time execution
- Uses `std::array` for fixed-size storage
- All operations are `constexpr`

**Runtime parser** (`lib/runTime/TomlParser.hpp`):
- Uses `std::vector` for dynamic storage
- Supports file I/O via `std::ifstream`

## String Utilities

Utility functions for string manipulation, parsing, and trimming.

### Compile-Time Utilities (`lib/compileTime/StringUtils.hpp`)

All `constexpr` for compile-time execution:

```cpp
namespace highp::config {

struct CompileTimeStringUtils {
    // Character checks
    static constexpr bool IsWhitespace(char c) noexcept;
    static constexpr bool IsDigit(char c) noexcept;

    // Trimming
    static constexpr std::string_view TrimLeft(std::string_view str) noexcept;
    static constexpr std::string_view TrimRight(std::string_view str) noexcept;
    static constexpr std::string_view Trim(std::string_view str) noexcept;

    // Searching
    static constexpr std::optional<size_t> Find(std::string_view str, char c) noexcept;
    static constexpr std::optional<size_t> FindFirst(std::string_view str, std::string_view chars) noexcept;
    static constexpr bool StartsWith(std::string_view str, std::string_view prefix) noexcept;
    static constexpr bool Equals(std::string_view a, std::string_view b) noexcept;

    // Parsing
    static constexpr std::optional<int> ParseInt(std::string_view str) noexcept;
    static constexpr std::optional<long> ParseLong(std::string_view str) noexcept;
    static constexpr std::optional<bool> ParseBool(std::string_view str) noexcept;

    // String processing
    static constexpr std::string_view Unquote(std::string_view str) noexcept;
};

} // namespace highp::config
```

### Runtime Utilities (`lib/runTime/StringUtils.hpp`)

Similar API but optimized for runtime:

```cpp
namespace highp::config {

struct StringUtils {
    // Same API as CompileTimeStringUtils, but:
    // - Uses std::string::find() instead of manual loops
    // - Returns std::string instead of std::string_view where appropriate
    // - Not constexpr (runtime-only)

    static bool IsWhitespace(char c) noexcept;
    static bool IsDigit(char c) noexcept;
    static std::string_view Trim(std::string_view str) noexcept;
    static std::optional<size_t> Find(std::string_view str, char c) noexcept;
    static bool StartsWith(std::string_view str, std::string_view prefix) noexcept;
    static std::optional<int> ParseInt(std::string_view str) noexcept;
    static std::optional<long> ParseLong(std::string_view str) noexcept;
    static std::optional<bool> ParseBool(std::string_view str) noexcept;
    static std::string Unquote(std::string_view str);  // Returns std::string
};

} // namespace highp::config
```

### Usage Examples

#### Compile-Time String Processing

```cpp
constexpr std::string_view configLine = "  port = 8080  # comment  ";
constexpr auto trimmed = CompileTimeStringUtils::Trim(configLine);
// trimmed == "port = 8080  # comment"

constexpr auto eqPos = CompileTimeStringUtils::Find(trimmed, '=');
// eqPos == 5

constexpr auto key = CompileTimeStringUtils::Trim(trimmed.substr(0, *eqPos));
// key == "port"

constexpr auto value = CompileTimeStringUtils::ParseInt("8080");
// value == 8080
```

#### Runtime Parsing

```cpp
std::string input = "  \"hello world\"  ";
std::string unquoted = StringUtils::Unquote(input);
// unquoted == "hello world"

auto port = StringUtils::ParseInt("  9000  ");
if (port.has_value()) {
    std::cout << "Port: " << *port << std::endl;  // Port: 9000
}

auto enabled = StringUtils::ParseBool("true");
if (*enabled) {
    std::cout << "Feature enabled" << std::endl;
}
```

## Error Handling

Centralized error code definitions and helper functions for logging and error propagation.

### Network Errors (`lib/NetworkError.h`)

Comprehensive enum for network operations:

```cpp
namespace highp::err {

enum class ENetworkError {
    UnknownError = 0,

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

// Convert error to human-readable string
constexpr std::string_view ToString(ENetworkError e);

} // namespace highp::err
```

### Error Helpers (`lib/Errors.hpp`)

Template functions for logging errors with `Result` integration:

```cpp
namespace highp::err {

// Convert error code to string (compile-time)
template <auto E>
constexpr std::string_view ErrToString() {
    return ToString(E);
}

// Log Windows Sockets API error + return Result
template <auto E>
static fn::Result<void, decltype(E)> LogErrorWSAWithResult(std::shared_ptr<log::Logger> logger) {
    logger->Error("{}: {}", ToString(E), WSAGetLastError());
    return fn::Result<void, decltype(E)>::Err(E);
}

// Log Windows API error + return Result
template <auto E>
static fn::Result<void, decltype(E)> LogErrorWindowsWithResult(std::shared_ptr<log::Logger> logger) {
    logger->Error("{}: {}", ToString(E), GetLastError());
    return fn::Result<void, decltype(E)>::Err(E);
}

// Log error message only
template <auto E>
static fn::Result<void, decltype(E)> LogErrorWithResult(std::shared_ptr<log::Logger> logger) {
    logger->Error("{}", ToString(E));
    return fn::Result<void, decltype(E)>::Err(E);
}

// Variants without Result return (void)
template <auto E>
static void LogErrorWSA(std::shared_ptr<log::Logger> logger);

template <auto E>
static void LogErrorWindows(std::shared_ptr<log::Logger> logger);

template <auto E>
static void LogError(std::shared_ptr<log::Logger> logger);

} // namespace highp::err
```

### Usage Examples

#### Basic Error Logging

```cpp
#include <Errors.hpp>
#include <NetworkError.h>

using namespace highp::err;

Result<void, ENetworkError> CreateSocket() {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        // Log WSA error code and return Result::Err
        return LogErrorWSAWithResult<ENetworkError::SocketCreateFailed>(logger);
    }
    return Result<void, ENetworkError>::Ok();
}
```

#### WSAStartup Error Handling

From `lib/Errors.hpp`:

```cpp
static fn::Result<void, ENetworkError> LogErrorByWSAStartupResult(
    std::shared_ptr<log::Logger> logger) {

    const int err = WSAGetLastError();
    switch (err) {
        case WSANOTINITIALISED:
            return LogErrorWSAWithResult<ENetworkError::WsaNotInitialized>(logger);
        case WSAENETDOWN:
            return LogErrorWSAWithResult<ENetworkError::WsaNetworkSubSystemFailed>(logger);
        case WSAEAFNOSUPPORT:
            return LogErrorWSAWithResult<ENetworkError::WsaNotSupportedAddressFamily>(logger);
        // ... more cases
        default:
            return LogErrorWSAWithResult<ENetworkError::WsaStartupFailed>(logger);
    }
}
```

#### Custom Error Enums

```cpp
namespace myapp {

enum class EConfigError {
    FileNotFound,
    ParseFailed,
    InvalidFormat,
};

constexpr std::string_view ToString(EConfigError e) {
    switch (e) {
        case EConfigError::FileNotFound: return "Config file not found";
        case EConfigError::ParseFailed: return "Failed to parse config";
        case EConfigError::InvalidFormat: return "Invalid config format";
        default: return "Unknown config error";
    }
}

Result<void, EConfigError> LoadConfig(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return LogErrorWithResult<EConfigError::FileNotFound>(logger);
    }
    // ...
}

} // namespace myapp
```

## Platform Abstractions

Platform-specific macros and type definitions for cross-platform compatibility.

### Macro Definitions (`lib/macro.h`)

```cpp
#pragma once

// Pure virtual function marker
#define PURE = 0

// Interface initialization boilerplate
#define INIT_INTERFACE(className) \
public: \
    virtual ~className() = default;
```

**Usage:**

```cpp
class IService {
    INIT_INTERFACE(IService)  // Expands to: public: virtual ~IService() = default;

    virtual void Start() PURE;  // Expands to: virtual void Start() = 0;
    virtual void Stop() PURE;
};
```

### Platform Header (`lib/framework.h`)

Windows-specific platform setup (currently implemented, Linux/Mac planned):

```cpp
#pragma once

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>

    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "mswsock.lib")
#endif

// Platform-specific types
#ifdef _WIN32
    using SocketHandle = SOCKET;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
    using SocketHandle = int;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif
```

**Future enhancements:**
- Linux epoll support (alternative to IOCP)
- macOS kqueue support
- Platform-agnostic socket types

## Performance Considerations

### Logger

- **String formatting overhead**: `std::format` has cost; avoid in hot paths
- **I/O blocking**: `TextLogger` uses blocking `std::cout`; consider async logger
- **Recommendation**: Use `Debug` logs only in debug builds with `#ifdef _DEBUG`

```cpp
#ifdef _DEBUG
    logger->Debug("Processing packet: {}", packetId);
#endif
```

### Result Type

- **Zero-cost in optimized builds**: Compilers inline and optimize away abstractions
- **No exceptions**: No stack unwinding overhead
- **Small storage**: Two fields + bool flag (typically 16 bytes for `Result<int, ENetworkError>`)
- **Copy semantics**: Returned by value; rely on RVO (Return Value Optimization)

### Object Pool

- **Mutex contention**: Single mutex for all operations
  - **Mitigation**: Use per-thread pools or lock-free alternatives
- **Memory footprint**: Pool never shrinks (by design)
  - **Tradeoff**: Predictable memory vs. memory reclamation
- **Pre-allocation**: Use `PreAllocate()` to avoid allocation during critical paths

**Benchmarking example:**

```cpp
// Without pool: ~500ns per allocation
for (int i = 0; i < 10000; ++i) {
    auto* buf = new Buffer();
    delete buf;
}

// With pool: ~50ns per acquire/release (10x faster)
ObjectPool<Buffer> pool(100);
for (int i = 0; i < 10000; ++i) {
    auto* buf = pool.Acquire();
    pool.Release(buf);
}
```

### TOML Parsers

- **Compile-time parser**: Zero runtime cost; results embedded in binary
  - **Limitation**: TOML content must be string literal (not file)
- **Runtime parser**: File I/O overhead on startup
  - **Optimization**: Cache `Config` instance; avoid repeated parsing

## Best Practices

### Logger Usage

1. **Dependency injection**: Pass `std::shared_ptr<Logger>` to classes
2. **Avoid over-logging**: Debug logs should be sparse in release builds
3. **Use format strings**: Prefer `Info("{}", value)` over string concatenation
4. **Exception logging**: Always log exceptions with context

```cpp
try {
    DoWork();
} catch (const std::exception& ex) {
    logger->Exception("Failed to process request", ex);
    throw;  // Re-throw if needed
}
```

### Result Type Usage

1. **Always check results**: `[[nodiscard]]` enforces this at compile-time
2. **Use explicit result checks**: Simplify error propagation
3. **Prefer early returns**: Reduce nesting with small `if` blocks
4. **Specific error types**: Create domain-specific error enums

```cpp
// Good: Early return with explicit checks
Result<void, ENetworkError> Initialize() {
    if (const auto result = Step1(); result.HasErr()) { return result; }
    if (const auto result = Step2(); result.HasErr()) { return result; }
    if (const auto result = Step3(); result.HasErr()) { return result; }
    return Result<void, ENetworkError>::Ok();
}

// Bad: Nested if-else
Result<void, ENetworkError> Initialize() {
    auto r1 = Step1();
    if (r1.IsOk()) {
        auto r2 = Step2();
        if (r2.IsOk()) {
            auto r3 = Step3();
            if (r3.IsOk()) {
                return Result<void, ENetworkError>::Ok();
            }
            return r3;
        }
        return r2;
    }
    return r1;
}
```

### Object Pool Usage

1. **Pre-allocate on startup**: Avoid allocations during request handling
2. **Reset objects on Release**: Ensure clean state for next use
3. **Document ownership**: Make clear who calls `Release()`
4. **Consider RAII wrapper**: Use RAII guard for automatic release

```cpp
// RAII wrapper for automatic release
template <typename T>
class PooledObject {
    ObjectPool<T>* _pool;
    T* _obj;
public:
    PooledObject(ObjectPool<T>& pool) : _pool(&pool), _obj(pool.Acquire()) {}
    ~PooledObject() { if (_obj) _pool->Release(_obj); }

    T* operator->() { return _obj; }
    T& operator*() { return *_obj; }
};

// Usage
void ProcessRequest(ObjectPool<Buffer>& bufferPool) {
    PooledObject<Buffer> buf(bufferPool);  // Auto-release on scope exit
    buf->Write(data, size);
}
```

### Configuration Best Practices

1. **Compile-time for constants**: Use `CompileTimeConfig` for unchanging values
2. **Runtime for deployment**: Use `Config` for environment-specific settings
3. **Environment variables**: Support env var overrides for containers/cloud
4. **Validation**: Validate config values at startup

```cpp
auto config = Config::FromFile("config.toml");
if (!config.has_value() || !config->IsLoaded()) {
    std::cerr << "Failed to load config" << std::endl;
    return -1;
}

int port = config->Int("server.port", 8080, "SERVER_PORT");
if (port < 1 || port > 65535) {
    std::cerr << "Invalid port: " << port << std::endl;
    return -1;
}
```

### Error Handling Best Practices

1. **Specific error codes**: Use descriptive enum values
2. **Log at error site**: Use `LogErrorWithResult` helpers
3. **Propagate with context**: Add context at each layer
4. **Don't swallow errors**: Always handle or propagate `Result`

```cpp
// Good: Context at each layer
Result<void, ENetworkError> EchoServer::Start() {
    auto res = _iocp.Start();
    if (res.HasErr()) {
        logger->Error("Failed to start IOCP: {}", ToString(res.Err()));
        return res;
    }
    logger->Info("IOCP started successfully");
    return Result<void, ENetworkError>::Ok();
}
```

### Thread Safety

1. **Logger**: Thread-safe (implementations must ensure this)
2. **Result**: Not thread-safe (return by value, no sharing)
3. **ObjectPool**: Thread-safe (mutex-protected)
4. **Config**: Thread-safe for reads (immutable after construction)

```cpp
// Safe: Each thread gets its own Result
std::jthread worker([logger]() {
    auto result = DoWork();  // Returns Result by value
    if (result.HasErr()) {
        logger->Error("Worker failed: {}", ToString(result.Err()));
    }
});

// Safe: Shared pool across threads
ObjectPool<Buffer> sharedPool(100);
std::vector<std::jthread> workers;
for (int i = 0; i < 4; ++i) {
    workers.emplace_back([&sharedPool]() {
        auto* buf = sharedPool.Acquire();  // Thread-safe
        // ... use buffer
        sharedPool.Release(buf);           // Thread-safe
    });
}
```

## Summary

The `lib/` utilities provide a robust foundation for high-performance server development:

- **Logger**: Flexible, format-string-based logging with backend polymorphism
- **Result**: Type-safe error handling without exceptions
- **ObjectPool**: Memory pooling for frequently allocated objects
- **TOML Config**: Compile-time and runtime configuration systems
- **String Utilities**: Parsing and manipulation for both constexpr and runtime contexts
- **Error Handling**: Centralized error codes with logging integration
- **Platform Abstractions**: Cross-platform compatibility layer (Windows now, Linux planned)

All utilities follow RAII principles, avoid hidden costs, and integrate seamlessly with the network layer. Use them together for clean, maintainable, high-performance code.

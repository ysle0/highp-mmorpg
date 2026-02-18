# Configuration Guide

> **Last Updated:** 2026-02-05
> **Status:** Complete - Dual Configuration System (Compile-time + Runtime)

---

## Table of Contents

1. [Configuration System Overview](#configuration-system-overview)
2. [Compile-time Configuration](#compile-time-configuration)
3. [Runtime Configuration](#runtime-configuration)
4. [TOML Parsing and Code Generation](#toml-parsing-and-code-generation)
5. [Configuration Loading Patterns](#configuration-loading-patterns)
6. [Best Practices](#best-practices)
7. [Performance Tuning Guidelines](#performance-tuning-guidelines)
8. [Configuration Examples](#configuration-examples)

---

## Configuration System Overview

The highp-mmorpg server uses a **dual configuration system** that separates compile-time constants from runtime-configurable values:

```
┌─────────────────────────────────────────────────────────────────┐
│                  Configuration Architecture                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌────────────────────┐              ┌────────────────────┐     │
│  │ config.compile.toml│              │config.runtime.toml │     │
│  │  (Buffer sizes)    │              │ (Server settings)  │     │
│  └─────────┬──────────┘              └─────────┬──────────┘     │
│            │                                   │                │
│            │ parse_compile_cfg.ps1             │                │
│            │                                   │                │
│            ▼                                   │                │
│  ┌────────────────────┐                       │                │
│  │    Const.h         │                       │                │
│  │  (constexpr)       │              parse_network_cfg.ps1     │
│  └────────────────────┘                       │                │
│            │                                   ▼                │
│            │                         ┌────────────────────┐     │
│            │                         │   NetworkCfg.h     │     │
│            │                         │  (runtime struct)  │     │
│            │                         └─────────┬──────────┘     │
│            │                                   │                │
│            │                                   │                │
│            │    ┌──────────────────┐           │                │
│            └────►  C++ Compiler    ◄───────────┘                │
│                 └────────┬─────────┘                            │
│                          │                                      │
│                          ▼                                      │
│                 ┌────────────────┐                              │
│                 │  echo-server   │                              │
│                 │  (executable)  │                              │
│                 └────────────────┘                              │
│                          │                                      │
│                          │ reads at runtime                     │
│                          ▼                                      │
│                 ┌────────────────┐                              │
│                 │config.runtime  │                              │
│                 │    .toml       │                              │
│                 └────────────────┘                              │
└─────────────────────────────────────────────────────────────────┘
```

### Why Two Configuration Systems?

#### Compile-time Configuration (`config.compile.toml` → `Const.h`)
- **Purpose:** Values that affect memory layout and must be known at compile time
- **Examples:** Buffer sizes, address buffer lengths, stack-allocated array sizes
- **Benefits:**
  - Zero runtime overhead (inlined as constants)
  - Type-safe constexpr values
  - Compiler optimizations enabled
- **Drawback:** Requires recompilation to change

#### Runtime Configuration (`config.runtime.toml` → `NetworkCfg.h`)
- **Purpose:** Values that can change between deployments without recompiling
- **Examples:** Port numbers, max clients, thread counts
- **Benefits:**
  - Change settings without rebuilding
  - Environment variable overrides for deployment flexibility
  - Hot-reload capability (if implemented)
- **Drawback:** Minimal runtime overhead for config access

---

## Compile-time Configuration

### Overview

Compile-time configuration is stored in `network/config.compile.toml` and generates `network/Const.h` containing `constexpr` C++ constants.

### File Location

```
highp-mmorpg/
└── network/
    ├── config.compile.toml  # Source TOML file
    └── Const.h              # Auto-generated header (DO NOT EDIT)
```

### Configuration File Format

**`network/config.compile.toml`:**

```toml
# Echo Server Compile-time Configuration
# These values are embedded at compile time for static buffer sizes.
# See config.runtime.toml for runtime-configurable values.

[buffer]
recv_buffer_size = 4096
send_buffer_size = 1024
address_buffer_size = 64
client_ip_buffer_size = 32
```

### Generated C++ Header

**`network/Const.h`** (auto-generated):

```cpp
#pragma once
// Auto-generated from: network/config.compile.toml
// Do not edit manually. Run scripts/parse_compile_cfg.ps1 to regenerate.

#include <Windows.h>

namespace highp::network {

/// <summary>
/// Compile-time network constants.
/// TOML sections are represented as nested structs.
/// </summary>
struct Const {
	/// <summary>[buffer] section</summary>
	struct Buffer {
		static constexpr INT recvBufferSize = 4096;
		static constexpr INT sendBufferSize = 1024;
		static constexpr INT addressBufferSize = 64;
		static constexpr INT clientIpBufferSize = 32;
	};
};

} // namespace highp::network
```

### Configuration Parameters

#### `[buffer]` Section

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `recv_buffer_size` | INT | 4096 | Size of receive buffer per client (bytes) |
| `send_buffer_size` | INT | 1024 | Size of send buffer per client (bytes) |
| `address_buffer_size` | INT | 64 | Size of address buffer for AcceptEx (bytes) |
| `client_ip_buffer_size` | INT | 32 | Size of client IP string buffer (bytes) |

#### Buffer Size Guidelines

**`recv_buffer_size`**
- **Purpose:** Per-client receive buffer for `WSARecv()`
- **Recommended:** 4096 bytes (4KB)
- **Considerations:**
  - Larger values reduce syscall overhead for large messages
  - Smaller values reduce per-client memory footprint
  - Must accommodate largest expected message + protocol overhead
  - Multiple of page size (4096) is optimal for memory alignment

**`send_buffer_size`**
- **Purpose:** Per-client send buffer for `WSASend()`
- **Recommended:** 1024 bytes (1KB) for echo server, larger for game server
- **Considerations:**
  - Echo server only echoes back received data (small buffer OK)
  - Game servers with frequent broadcasts need larger buffers
  - Consider message serialization size (FlatBuffers, etc.)

**`address_buffer_size`**
- **Purpose:** Buffer for local/remote address information in `AcceptEx()`
- **Recommended:** 64 bytes (sockaddr_in6 + 16 bytes padding per spec)
- **Considerations:**
  - Must be at least `sizeof(sockaddr_in6) + 16` per Microsoft spec
  - Fixed by Winsock API requirements
  - Rarely needs adjustment

**`client_ip_buffer_size`**
- **Purpose:** String buffer for client IP address (e.g., "192.168.1.100")
- **Recommended:** 32 bytes (enough for IPv6 addresses)
- **Considerations:**
  - IPv4: Max 15 chars ("255.255.255.255")
  - IPv6: Max 39 chars + null terminator
  - 32 bytes handles both with safety margin

### When to Use Compile-time Configuration

Use compile-time configuration when:

1. **Memory Layout Dependency**: Value affects struct size or stack allocation
   ```cpp
   struct Client {
       char recvBuffer[Const::Buffer::recvBufferSize]; // Stack allocation
       char sendBuffer[Const::Buffer::sendBufferSize];
   };
   ```

2. **Performance Critical**: Value is accessed in hot paths
   ```cpp
   // Inlined as constant - no runtime lookup
   WSARecv(socket, ..., Const::Buffer::recvBufferSize, ...);
   ```

3. **Compile-time Validation**: Value enables compile-time size checks
   ```cpp
   static_assert(Const::Buffer::recvBufferSize >= 1024,
                 "Receive buffer too small");
   ```

4. **Rare Changes**: Value rarely changes across deployments

### Auto-generation Process

**Script:** `scripts/parse_compile_cfg.ps1`

**Execution:**

```powershell
# Run from project root
cd C:\Users\dlrnt\werk\highp-mmorpg
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
```

**Output:**

```
Generated: C:\Users\dlrnt\werk\highp-mmorpg\network\Const.h
```

**When to Regenerate:**

1. After modifying `network/config.compile.toml`
2. Before building after pulling changes
3. As part of pre-build step (can be automated in MSBuild)

**Important:** The generated `Const.h` file is checked into source control to ensure build reproducibility without requiring PowerShell execution.

---

## Runtime Configuration

### Overview

Runtime configuration is stored in `exec/echo/echo-server/config.runtime.toml` and generates `network/NetworkCfg.h` containing a runtime configuration struct with loading methods.

### File Locations

```
highp-mmorpg/
├── exec/
│   └── echo/
│       └── echo-server/
│           └── config.runtime.toml  # Source TOML file
└── network/
    └── NetworkCfg.h                 # Auto-generated header
```

### Configuration File Format

**`exec/echo/echo-server/config.runtime.toml`:**

```toml
# Echo Server Runtime Configuration
# These values can be changed without recompilation.
# Environment variables override: SERVER_PORT, SERVER_MAX_CLIENTS, etc.

[server]
port = 8080
max_clients = 1000
backlog = 10

[thread]
max_worker_thread_multiplier = 2
max_acceptor_thread_count = 2
desirable_iocp_thread_count = -1
```

### Generated C++ Header

**`network/NetworkCfg.h`** (auto-generated, simplified):

```cpp
#pragma once
// Auto-generated from: echo/echo-server/config.runtime.toml
// Do not edit manually. Run scripts/parse_network_cfg.ps1 to regenerate.

#include <filesystem>
#include <runTime/Config.hpp>
#include <stdexcept>
#include <Windows.h>

namespace highp::network {

/// <summary>
/// Runtime network configuration.
/// TOML sections are represented as nested structs.
/// </summary>
struct NetworkCfg {
	/// <summary>[server] section</summary>
	struct Server {
		INT port;
		INT maxClients;
		INT backlog;

		struct Defaults {
			static constexpr INT port = 8080;
			static constexpr INT maxClients = 1000;
			static constexpr INT backlog = 10;
		};
	} server;

	/// <summary>[thread] section</summary>
	struct Thread {
		INT maxWorkerThreadMultiplier;
		INT maxAcceptorThreadCount;
		INT desirableIocpThreadCount;

		struct Defaults {
			static constexpr INT maxWorkerThreadMultiplier = 2;
			static constexpr INT maxAcceptorThreadCount = 2;
			static constexpr INT desirableIocpThreadCount = -1;
		};
	} thread;

	/// <summary>Load from TOML file</summary>
	static NetworkCfg FromFile(const std::filesystem::path& path);

	/// <summary>Load from Config object</summary>
	static NetworkCfg FromCfg(const highp::config::Config& cfg);

	/// <summary>Create with default values</summary>
	static NetworkCfg WithDefaults();
};

} // namespace highp::network
```

### Configuration Parameters

#### `[server]` Section

| Parameter | Type | Default | Environment Variable | Description |
|-----------|------|---------|---------------------|-------------|
| `port` | INT | 8080 | `SERVER_PORT` | TCP port to listen on |
| `max_clients` | INT | 1000 | `SERVER_MAX_CLIENTS` | Maximum concurrent client connections |
| `backlog` | INT | 10 | `SERVER_BACKLOG` | Listen socket backlog queue size |

**Parameter Details:**

- **`port`**: TCP port number (1-65535)
  - Development: 8080-8090
  - Production: 80 (HTTP), 443 (HTTPS), or custom (e.g., 7777)
  - Requires admin privileges for ports < 1024 on Windows

- **`max_clients`**: Maximum concurrent connections
  - Echo server: 1000 (development), 10000+ (production)
  - Limited by: Available memory, socket handles, IOCP worker threads
  - Each client consumes: ~8KB (buffers) + 2KB (overhead) ≈ 10KB
  - 10000 clients ≈ 100MB memory

- **`backlog`**: Listen queue size for `listen()`
  - Typical: 10-128 (Windows default: SOMAXCONN = 2147483647)
  - Higher values handle connection bursts
  - Rarely needs tuning unless high connection rate

#### `[thread]` Section

| Parameter | Type | Default | Environment Variable | Description |
|-----------|------|---------|---------------------|-------------|
| `max_worker_thread_multiplier` | INT | 2 | `THREAD_MAX_WORKER_THREAD_MULTIPLIER` | Worker threads = cores × multiplier |
| `max_acceptor_thread_count` | INT | 2 | `THREAD_MAX_ACCEPTOR_THREAD_COUNT` | Number of acceptor threads |
| `desirable_iocp_thread_count` | INT | -1 | `THREAD_DESIRABLE_IOCP_THREAD_COUNT` | IOCP concurrent thread count (-1 = auto) |

**Parameter Details:**

- **`max_worker_thread_multiplier`**:
  - Formula: `workerThreads = std::thread::hardware_concurrency() × multiplier`
  - Typical: 1-2 (CPU-bound), 2-4 (I/O-bound)
  - Echo server: 2 (balanced I/O workload)
  - Game server: 2-4 (more I/O operations)
  - **Warning:** Too many threads cause context switching overhead

- **`max_acceptor_thread_count`**:
  - Number of threads running `AcceptEx()` loops
  - Typical: 1-4 (rarely needs more than 2)
  - Higher values improve connection accept throughput
  - **Warning:** Diminishing returns beyond 2-4 threads

- **`desirable_iocp_thread_count`**:
  - Concurrent threads allowed to execute in IOCP
  - -1: Auto-detect (recommended)
  - 0: System default (number of processors)
  - >0: Explicit count
  - **Recommendation:** Leave at -1 unless profiling shows contention

### Environment Variable Overrides

Runtime configuration supports **environment variable overrides** for deployment flexibility without modifying TOML files.

#### Override Mechanism

1. **Priority:** Environment variable > TOML value > Default value
2. **Naming Convention:** `SECTION_PARAMETER` (uppercase, snake_case → uppercase)
3. **Type Conversion:** Automatic parsing (int, bool, string)

#### Example Usage

**Bash/Shell:**

```bash
# Override port and max clients
export SERVER_PORT=9090
export SERVER_MAX_CLIENTS=5000
./echo-server.exe
```

**PowerShell:**

```powershell
# Override port and max clients
$env:SERVER_PORT = "9090"
$env:SERVER_MAX_CLIENTS = "5000"
.\echo-server.exe
```

**Docker/Kubernetes:**

```yaml
# docker-compose.yml
services:
  echo-server:
    image: highp-echo-server
    environment:
      - SERVER_PORT=8080
      - SERVER_MAX_CLIENTS=10000
      - THREAD_MAX_WORKER_THREAD_MULTIPLIER=4
```

#### Available Environment Variables

| Environment Variable | TOML Key | Type |
|---------------------|----------|------|
| `SERVER_PORT` | `server.port` | INT |
| `SERVER_MAX_CLIENTS` | `server.max_clients` | INT |
| `SERVER_BACKLOG` | `server.backlog` | INT |
| `THREAD_MAX_WORKER_THREAD_MULTIPLIER` | `thread.max_worker_thread_multiplier` | INT |
| `THREAD_MAX_ACCEPTOR_THREAD_COUNT` | `thread.max_acceptor_thread_count` | INT |
| `THREAD_DESIRABLE_IOCP_THREAD_COUNT` | `thread.desirable_iocp_thread_count` | INT |

### Auto-generation Process

**Script:** `scripts/parse_network_cfg.ps1`

**Execution:**

```powershell
# Run from project root
cd C:\Users\dlrnt\werk\highp-mmorpg
powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1
```

**Output:**

```
Generated: C:\Users\dlrnt\werk\highp-mmorpg\network\NetworkCfg.h
```

**When to Regenerate:**

1. After modifying `exec/echo/echo-server/config.runtime.toml`
2. After adding/removing configuration parameters
3. Before building after pulling changes
4. As part of pre-build step (can be automated)

---

## TOML Parsing and Code Generation

### PowerShell Script Mechanics

Both configuration scripts (`parse_compile_cfg.ps1` and `parse_network_cfg.ps1`) follow a similar pipeline:

```
┌─────────────────────────────────────────────────────────────┐
│              TOML to C++ Code Generation Pipeline           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. Read TOML File                                          │
│     ├─ Get-Content -Path $TomlPath                          │
│     └─ Parse line-by-line (sections and key-value pairs)    │
│                                                             │
│  2. Parse Structure                                         │
│     ├─ Detect sections: [section_name]                      │
│     ├─ Extract key-value pairs: key = value                 │
│     └─ Store in hashtable: $sections[section][entries]      │
│                                                             │
│  3. Type Inference                                          │
│     ├─ Integer: /^-?\d+$/                                   │
│     ├─ Double: /^-?\d+\.\d+$/                               │
│     ├─ Boolean: true|false                                  │
│     └─ String: default (const char* or std::string)         │
│                                                             │
│  4. Naming Convention Conversion                            │
│     ├─ snake_case → PascalCase (struct names)               │
│     └─ snake_case → camelCase (field names)                 │
│                                                             │
│  5. C++ Code Generation                                     │
│     ├─ Generate namespace and struct declarations           │
│     ├─ Generate constexpr/member fields                     │
│     ├─ Generate Defaults nested struct (runtime only)       │
│     └─ Generate FromFile/FromCfg/WithDefaults (runtime only)│
│                                                             │
│  6. Write Output File                                       │
│     ├─ Out-File -FilePath $OutputPath -Encoding UTF8        │
│     └─ Include auto-generation warning header               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Naming Conventions

The scripts automatically convert TOML snake_case to C++ naming conventions:

#### Snake Case → PascalCase (Struct Names)

```powershell
function ConvertTo-PascalCase {
    param([string]$text)
    $parts = $text -split '_'
    $result = ($parts | ForEach-Object {
        if ($_.Length -gt 0) {
            $_.Substring(0,1).ToUpper() + $_.Substring(1).ToLower()
        }
    }) -join ''
    return $result
}
```

**Examples:**
- `buffer` → `Buffer`
- `server` → `Server`
- `thread` → `Thread`
- `max_worker_thread_multiplier` → `MaxWorkerThreadMultiplier`

#### Snake Case → camelCase (Field Names)

```powershell
function ConvertTo-CamelCase {
    param([string]$text)
    $pascal = ConvertTo-PascalCase $text
    if ($pascal.Length -gt 0) {
        return $pascal.Substring(0,1).ToLower() + $pascal.Substring(1)
    }
    return $pascal
}
```

**Examples:**
- `recv_buffer_size` → `recvBufferSize`
- `port` → `port`
- `max_clients` → `maxClients`
- `desirable_iocp_thread_count` → `desirableIocpThreadCount`

### Type Inference

The scripts infer C++ types from TOML values using regex patterns:

```powershell
function Get-CppType {
    param([string]$value)
    if ($value -match '^-?\d+$') { return "INT" }            # Integer
    if ($value -match '^-?\d+\.\d+$') { return "double" }    # Floating point
    if ($value -eq 'true' -or $value -eq 'false') { return "bool" }  # Boolean
    return "const char*"  # Compile-time (string literal)
    # OR
    return "std::string"  # Runtime (string object)
}
```

**Type Mapping:**

| TOML Value | Regex Match | C++ Type (Compile) | C++ Type (Runtime) |
|------------|-------------|-------------------|-------------------|
| `4096` | `^-?\d+$` | `INT` | `INT` |
| `-1` | `^-?\d+$` | `INT` | `INT` |
| `3.14` | `^-?\d+\.\d+$` | `double` | `double` |
| `true` | `true\|false` | `bool` | `bool` |
| `"hello"` | (default) | `const char*` | `std::string` |

### Code Generation Patterns

#### Compile-time Configuration (Const.h)

```cpp
struct Const {
	struct [PascalCase Section] {
		static constexpr [Type] [camelCase field] = [value];
		// ...
	};
};
```

**Example:**

```toml
[buffer]
recv_buffer_size = 4096
```

Generates:

```cpp
struct Const {
	struct Buffer {
		static constexpr INT recvBufferSize = 4096;
	};
};
```

#### Runtime Configuration (NetworkCfg.h)

```cpp
struct NetworkCfg {
	struct [PascalCase Section] {
		[Type] [camelCase field];
		// ...

		struct Defaults {
			static constexpr [Type] [camelCase field] = [value];
			// ...
		};
	} [sectionName];

	static NetworkCfg FromFile(const std::filesystem::path& path);
	static NetworkCfg FromCfg(const highp::config::Config& cfg);
	static NetworkCfg WithDefaults();
};
```

**Example:**

```toml
[server]
port = 8080
```

Generates:

```cpp
struct NetworkCfg {
	struct Server {
		INT port;

		struct Defaults {
			static constexpr INT port = 8080;
		};
	} server;

	// Factory methods...
};
```

### Limitations

1. **Single-level Sections:** Only `[section]` supported, not `[section.subsection]`
2. **Simple Values:** No arrays, tables, or multi-line strings
3. **No Comments Preservation:** Comments are not carried into generated code
4. **No Validation:** Invalid TOML syntax may produce incorrect output
5. **Integer Type:** All integers map to `INT` (Windows `int` type)

---

## Configuration Loading Patterns

### Runtime Configuration Loading

The generated `NetworkCfg` struct provides three factory methods for loading configuration:

#### 1. FromFile() - Load from TOML File

**Signature:**

```cpp
static NetworkCfg FromFile(const std::filesystem::path& path);
```

**Usage:**

```cpp
#include <network/NetworkCfg.h>

// Load from TOML file (with environment variable overrides)
auto cfg = highp::network::NetworkCfg::FromFile("config.runtime.toml");

// Access configuration values
int port = cfg.server.port;
int maxClients = cfg.server.maxClients;
int workerThreads = cfg.thread.maxWorkerThreadMultiplier * std::thread::hardware_concurrency();
```

**Behavior:**

1. Loads TOML file using `highp::config::Config::FromFile()`
2. Applies environment variable overrides
3. Falls back to default values if key not found
4. Throws `std::runtime_error` if file cannot be loaded

**Error Handling:**

```cpp
try {
    auto cfg = NetworkCfg::FromFile("config.runtime.toml");
    // Use cfg...
} catch (const std::runtime_error& e) {
    std::cerr << "Config load failed: " << e.what() << std::endl;
    // Fall back to defaults or exit
}
```

#### 2. FromCfg() - Load from Config Object

**Signature:**

```cpp
static NetworkCfg FromCfg(const highp::config::Config& cfg);
```

**Usage:**

```cpp
// Load Config object manually
auto configOpt = highp::config::Config::FromFile("config.runtime.toml");
if (configOpt.has_value()) {
    auto cfg = highp::network::NetworkCfg::FromCfg(configOpt.value());
    // Use cfg...
}
```

**Behavior:**

1. Accepts pre-loaded `Config` object
2. Applies environment variable overrides
3. Falls back to default values if key not found
4. Does not throw (assumes valid Config object)

**When to Use:**

- When you need custom TOML parsing logic
- When loading multiple configurations from same file
- When you want to avoid exception handling

#### 3. WithDefaults() - Use Default Values

**Signature:**

```cpp
static NetworkCfg WithDefaults();
```

**Usage:**

```cpp
// Create configuration with all default values (no file, no env vars)
auto cfg = highp::network::NetworkCfg::WithDefaults();

// Modify as needed
cfg.server.port = 9090;
cfg.server.maxClients = 5000;
```

**Behavior:**

1. Creates `NetworkCfg` with all default values from `Defaults` nested struct
2. No file I/O
3. No environment variable overrides
4. Useful for testing or embedded scenarios

### Compile-time Configuration Access

Compile-time constants are accessed directly via `constexpr` members:

```cpp
#include <network/Const.h>

// Access compile-time constants (inlined by compiler)
constexpr int recvSize = highp::network::Const::Buffer::recvBufferSize;  // 4096
constexpr int sendSize = highp::network::Const::Buffer::sendBufferSize;  // 1024

// Use in array declarations
char recvBuffer[highp::network::Const::Buffer::recvBufferSize];
```

### Environment Variable Override Internals

The `Config` class handles environment variable overrides transparently:

```cpp
// From lib/runTime/Config.hpp
[[nodiscard]] int Int(
    std::string_view key,
    int defaultValue = 0,
    const char* envVar = nullptr) const {

    // 1. Check environment variable first
    if (envVar != nullptr) {
        if (auto env = GetEnv(envVar); !env.empty()) {
            if (auto parsed = StringUtils::ParseInt(env)) {
                return parsed.value();  // Environment variable wins
            }
        }
    }

    // 2. Check TOML value
    auto value = _result.FindInt(key);
    return value.value_or(defaultValue);  // TOML or default
}
```

**Priority Order:**

1. Environment variable (highest priority)
2. TOML file value
3. Default value (lowest priority)

---

## Best Practices

### 1. Compile-time vs Runtime Configuration

**Use Compile-time For:**
- Buffer sizes and memory layout
- Protocol version numbers
- Compile-time assertions
- Performance-critical constants

**Use Runtime For:**
- Server ports and addresses
- Client limits and timeouts
- Thread pool sizes
- Feature flags and logging levels

### 2. Configuration Management

**DO:**
- ✅ Keep configuration files in source control (except secrets)
- ✅ Use environment variables for deployment-specific settings
- ✅ Document all configuration parameters in comments
- ✅ Provide sensible defaults for all values
- ✅ Validate configuration at startup
- ✅ Regenerate headers after TOML changes

**DON'T:**
- ❌ Edit generated headers manually (changes will be overwritten)
- ❌ Store secrets (passwords, keys) in TOML files
- ❌ Use runtime config for values affecting memory layout
- ❌ Commit environment-specific TOML files (use env vars instead)
- ❌ Mix compile-time and runtime concerns

### 3. Version Control

**Files to Commit:**
- ✅ `config.compile.toml` (source)
- ✅ `config.runtime.toml` (template/defaults)
- ✅ `Const.h` (generated, for build reproducibility)
- ✅ `NetworkCfg.h` (generated, for build reproducibility)
- ✅ `parse_compile_cfg.ps1` (generator script)
- ✅ `parse_network_cfg.ps1` (generator script)

**Files to Ignore:**
- ❌ `config.runtime.local.toml` (developer overrides)
- ❌ `.env` files (environment variables)
- ❌ Deployment-specific TOML files

### 4. Configuration Validation

**Startup Validation Example:**

```cpp
void ValidateConfig(const NetworkCfg& cfg) {
    // Validate port range
    if (cfg.server.port < 1 || cfg.server.port > 65535) {
        throw std::runtime_error("Invalid port: " + std::to_string(cfg.server.port));
    }

    // Validate max clients
    if (cfg.server.maxClients < 1 || cfg.server.maxClients > 100000) {
        throw std::runtime_error("Invalid max_clients: " + std::to_string(cfg.server.maxClients));
    }

    // Validate thread multiplier
    if (cfg.thread.maxWorkerThreadMultiplier < 1 || cfg.thread.maxWorkerThreadMultiplier > 8) {
        throw std::runtime_error("Invalid worker multiplier: " +
                                 std::to_string(cfg.thread.maxWorkerThreadMultiplier));
    }

    // Log validated configuration
    Logger::Info("Configuration validated successfully");
    Logger::Info("  Port: {}", cfg.server.port);
    Logger::Info("  Max clients: {}", cfg.server.maxClients);
    Logger::Info("  Worker threads: {}",
                 cfg.thread.maxWorkerThreadMultiplier * std::thread::hardware_concurrency());
}
```

### 5. Hot Reload Support (Future)

**Design Pattern for Hot Reload:**

```cpp
class ConfigurableServer {
private:
    std::atomic<NetworkCfg*> _cfg;  // Atomic pointer swap
    std::filesystem::path _configPath;

public:
    void ReloadConfig() {
        auto newCfg = NetworkCfg::FromFile(_configPath);
        ValidateConfig(newCfg);

        auto oldCfg = _cfg.exchange(new NetworkCfg(newCfg));
        delete oldCfg;

        Logger::Info("Configuration reloaded");
    }

    const NetworkCfg& GetConfig() const {
        return *_cfg.load();
    }
};
```

---

## Performance Tuning Guidelines

### Buffer Size Tuning

#### Receive Buffer (`recv_buffer_size`)

**Tuning Strategy:**

1. **Profile Message Sizes:**
   ```cpp
   // Log message sizes to find 95th percentile
   Logger::Debug("Recv message size: {}", bytesReceived);
   ```

2. **Set Buffer to 95th Percentile:**
   - Too small: Messages truncated or require multiple reads
   - Too large: Wasted memory per client
   - Sweet spot: 95th percentile + 20% headroom

3. **Memory vs Performance Trade-off:**
   - 1000 clients × 4KB = 4MB (reasonable)
   - 10000 clients × 4KB = 40MB (acceptable)
   - 10000 clients × 64KB = 640MB (too much)

**Recommendations:**

| Scenario | Recv Buffer Size | Rationale |
|----------|-----------------|-----------|
| Echo Server | 4KB | Small messages, low throughput |
| Chat Server | 8KB | Medium messages (text + metadata) |
| Game Server | 16KB | Batched commands, position updates |
| File Transfer | 64KB | Large payloads, high throughput |

#### Send Buffer (`send_buffer_size`)

**Tuning Strategy:**

1. **Match Application Message Size:**
   - Echo: Same as recv buffer (1KB)
   - Chat: Average message size (2-4KB)
   - Game: Batched updates (8-16KB)

2. **Consider Protocol Overhead:**
   - FlatBuffers: Minimal overhead (<1%)
   - JSON: 20-50% overhead
   - Add 10-20% buffer for safety

### Thread Pool Tuning

#### Worker Thread Multiplier

**Tuning Strategy:**

1. **CPU-bound Workload:**
   - Multiplier: 1 (one thread per core)
   - Example: Game logic calculations, AI processing

2. **I/O-bound Workload:**
   - Multiplier: 2-4 (more threads than cores)
   - Example: Database queries, external API calls

3. **Mixed Workload:**
   - Multiplier: 2 (balanced approach)
   - Example: Echo server, chat server

**Profiling Commands:**

```powershell
# Monitor CPU usage
Get-Counter '\Processor(_Total)\% Processor Time'

# Monitor thread count
Get-Process echo-server | Select-Object -ExpandProperty Threads | Measure-Object | Select-Object -ExpandProperty Count
```

**Warning Signs:**

- High CPU, low throughput → Too many threads (context switching)
- Low CPU, high latency → Too few threads (underutilized)

#### IOCP Thread Count

**Recommendation:** Use `-1` (auto-detect) unless profiling shows otherwise.

**Manual Tuning (Advanced):**

1. **Set to Number of Cores:**
   - `desirable_iocp_thread_count = 8` (for 8-core CPU)
   - Prevents thread starvation on IOCP

2. **Set Higher for Blocking Operations:**
   - If handlers perform blocking I/O (database, file I/O)
   - `desirable_iocp_thread_count = 16` (2× cores)

3. **Monitor IOCP Contention:**
   - Use Windows Performance Monitor → IOCP object counters
   - Look for high wait times

### Connection Limit Tuning

#### Max Clients

**Calculation:**

```
Total Memory = maxClients × (recvBuffer + sendBuffer + overhead)
             = 10000 × (4KB + 1KB + 5KB)
             = 100MB
```

**System Limits:**

- **Windows Socket Handles:** ~64000 per process (configurable)
- **Physical Memory:** Each client needs ~10KB
- **IOCP Performance:** Degrades beyond ~100K concurrent connections

**Recommendations:**

| Hardware | Max Clients | Notes |
|----------|------------|-------|
| 8GB RAM, 4 cores | 5000 | Development/testing |
| 16GB RAM, 8 cores | 20000 | Small production |
| 32GB RAM, 16 cores | 50000 | Medium production |
| 64GB RAM, 32 cores | 100000 | Large production |

### Backlog Tuning

**Recommendation:** Start with 10-128, increase if connection drops occur.

**Tuning Process:**

1. **Monitor Connection Failures:**
   ```cpp
   if (accept() == SOCKET_ERROR && WSAGetLastError() == WSAECONNREFUSED) {
       Logger::Warn("Connection refused - backlog full?");
   }
   ```

2. **Increase Backlog Gradually:**
   - Start: 10
   - Medium load: 64
   - High load: 128-512

3. **Verify with Load Testing:**
   ```powershell
   # Simulate 1000 concurrent connections
   1..1000 | ForEach-Object -Parallel {
       .\echo-client.exe
   }
   ```

---

## Configuration Examples

### Development Environment

**`config.runtime.toml`:**

```toml
# Development configuration - local testing with verbose logging

[server]
port = 8080              # Non-privileged port
max_clients = 100        # Low limit for debugging
backlog = 10             # Sufficient for local testing

[thread]
max_worker_thread_multiplier = 1   # Minimal threads for debugging
max_acceptor_thread_count = 1      # Single acceptor thread
desirable_iocp_thread_count = -1   # Auto-detect
```

**Environment Variables:**

```powershell
# None needed - use TOML defaults
```

### Staging Environment

**`config.runtime.toml`:**

```toml
# Staging configuration - production-like settings

[server]
port = 8080
max_clients = 5000       # Moderate limit
backlog = 64             # Handle connection bursts

[thread]
max_worker_thread_multiplier = 2   # Balanced I/O
max_acceptor_thread_count = 2      # Dual acceptor threads
desirable_iocp_thread_count = -1   # Auto-detect
```

**Environment Variables:**

```bash
# Override for specific staging host
export SERVER_PORT=8081
export SERVER_MAX_CLIENTS=10000
```

### Production Environment

**`config.runtime.toml`:**

```toml
# Production configuration - high performance

[server]
port = 7777              # Custom game server port
max_clients = 20000      # High capacity
backlog = 128            # Large connection queue

[thread]
max_worker_thread_multiplier = 3   # High I/O workload
max_acceptor_thread_count = 4      # Multiple acceptor threads
desirable_iocp_thread_count = -1   # Auto-detect
```

**Environment Variables (Docker/Kubernetes):**

```yaml
# docker-compose.yml
services:
  game-server:
    image: highp-game-server:latest
    environment:
      - SERVER_PORT=7777
      - SERVER_MAX_CLIENTS=50000
      - THREAD_MAX_WORKER_THREAD_MULTIPLIER=4
    deploy:
      resources:
        limits:
          cpus: '16'
          memory: 8G
```

### Low-Memory Environment (Embedded/Edge)

**`config.compile.toml`:**

```toml
[buffer]
recv_buffer_size = 1024  # Reduce buffer sizes
send_buffer_size = 512
address_buffer_size = 64
client_ip_buffer_size = 32
```

**`config.runtime.toml`:**

```toml
[server]
port = 8080
max_clients = 100        # Low client limit
backlog = 5

[thread]
max_worker_thread_multiplier = 1   # Minimal threads
max_acceptor_thread_count = 1
desirable_iocp_thread_count = -1
```

### High-Throughput Environment (Chat/Broadcasting)

**`config.compile.toml`:**

```toml
[buffer]
recv_buffer_size = 8192   # Larger buffers for batching
send_buffer_size = 8192
address_buffer_size = 64
client_ip_buffer_size = 32
```

**`config.runtime.toml`:**

```toml
[server]
port = 9090
max_clients = 10000
backlog = 128

[thread]
max_worker_thread_multiplier = 4   # High I/O concurrency
max_acceptor_thread_count = 4
desirable_iocp_thread_count = -1
```

---

## Configuration File Templates

### Minimal Template

```toml
# Minimal configuration - uses defaults for most values

[server]
port = 8080

[thread]
max_worker_thread_multiplier = 2
```

### Fully Documented Template

```toml
# highp-mmorpg Server Configuration
# Environment variables override TOML values (see docs/CONFIGURATION.md)

[server]
# TCP port to bind (1-65535)
# Env: SERVER_PORT
port = 8080

# Maximum concurrent client connections
# Memory usage: ~10KB per client
# Env: SERVER_MAX_CLIENTS
max_clients = 1000

# Listen socket backlog queue size
# Increase if connection drops occur under load
# Env: SERVER_BACKLOG
backlog = 10

[thread]
# Worker thread count = hardware_concurrency × multiplier
# CPU-bound: 1, I/O-bound: 2-4, Balanced: 2
# Env: THREAD_MAX_WORKER_THREAD_MULTIPLIER
max_worker_thread_multiplier = 2

# Number of AcceptEx() threads
# Typical: 1-4 (diminishing returns beyond 2)
# Env: THREAD_MAX_ACCEPTOR_THREAD_COUNT
max_acceptor_thread_count = 2

# IOCP concurrent thread count
# -1: Auto-detect (recommended), 0: System default, >0: Explicit
# Env: THREAD_DESIRABLE_IOCP_THREAD_COUNT
desirable_iocp_thread_count = -1
```

---

## Troubleshooting

### Configuration Not Loading

**Symptoms:**
- Server uses default values despite TOML file present
- Environment variables not applied

**Solutions:**

1. **Check File Path:**
   ```cpp
   // Log actual config path
   auto absPath = std::filesystem::absolute("config.runtime.toml");
   Logger::Info("Loading config from: {}", absPath.string());
   ```

2. **Verify Working Directory:**
   ```powershell
   # Run from directory containing config.runtime.toml
   cd exec\echo\echo-server
   ..\..\..\x64\Release\echo-server.exe
   ```

3. **Check Environment Variables:**
   ```powershell
   # Verify env vars are set
   Get-ChildItem Env: | Where-Object { $_.Name -like "SERVER_*" -or $_.Name -like "THREAD_*" }
   ```

### Generated Headers Out of Sync

**Symptoms:**
- Build errors referencing config fields
- Intellisense errors in headers

**Solutions:**

1. **Regenerate Headers:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
   powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1
   ```

2. **Clean and Rebuild:**
   ```powershell
   msbuild highp-mmorpg.slnx /t:Clean
   msbuild highp-mmorpg.slnx /t:Rebuild
   ```

### TOML Parsing Errors

**Symptoms:**
- Generated header has incorrect values
- Build warnings about type mismatches

**Solutions:**

1. **Validate TOML Syntax:**
   - Check for typos in section names: `[server]` not `[Server]`
   - Ensure proper key-value format: `port = 8080` not `port=8080` (spaces matter)
   - Remove trailing commas and quotes

2. **Check Type Inference:**
   - Integers: `4096` not `"4096"`
   - Booleans: `true` not `"true"`
   - Strings: `"hello"` with quotes

---

## Cross-References

### Related Documentation

- **[Architecture Overview](ARCHITECTURE.md)** - System design and layer separation
- **[Network Layer Guide](NETWORK_LAYER.md)** - IOCP and async I/O implementation
- **[API Reference](API_REFERENCE.md)** - Config class API documentation
- **[Building and Testing](BUILD_AND_TEST.md)** - Build instructions with config regeneration

### Source Files

- `network/config.compile.toml` - Compile-time configuration source
- `exec/echo/echo-server/config.runtime.toml` - Runtime configuration source
- `scripts/parse_compile_cfg.ps1` - Compile-time config generator
- `scripts/parse_network_cfg.ps1` - Runtime config generator
- `network/Const.h` - Generated compile-time constants
- `network/NetworkCfg.h` - Generated runtime configuration struct
- `lib/runTime/Config.hpp` - Configuration loader implementation

---

**Last Updated:** 2026-02-05
**Maintained By:** Project Team

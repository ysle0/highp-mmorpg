# Build and Test Guide

This document provides comprehensive instructions for building, configuring, and testing the highp-mmorpg project.

## Table of Contents

1. [Build System Overview](#build-system-overview)
2. [Solution Structure](#solution-structure)
3. [Prerequisites and Setup](#prerequisites-and-setup)
4. [Build Commands](#build-commands)
5. [Build Configurations](#build-configurations)
6. [Building Individual Projects](#building-individual-projects)
7. [Configuration Regeneration](#configuration-regeneration)
8. [Testing Approach](#testing-approach)
9. [Running the Echo Server and Client](#running-the-echo-server-and-client)
10. [Troubleshooting Common Build Issues](#troubleshooting-common-build-issues)
11. [CI/CD Considerations](#cicd-considerations)

## Build System Overview

### Technology Stack

- **IDE**: Visual Studio 2022
- **Build System**: MSBuild (Visual Studio's native build engine)
- **C++ Standard**: C++17/C++20
- **Platform**: Windows (IOCP-based architecture)
- **Architecture**: x64 (primary), x86 (supported)

### Build Process

The project uses MSBuild with `.vcxproj` project files organized under a solution file (`highp-mmorpg.slnx`). The build process includes:

1. **Pre-build**: Configuration header generation from TOML files
2. **Compilation**: C++ source compilation with appropriate standards
3. **Linking**: Static libraries linked into executables
4. **Post-build**: Optional FlatBuffers schema compilation

## Solution Structure

The solution is organized in `highp-mmorpg.slnx` with the following structure:

```
highp-mmorpg.slnx
├── /shared/                    # Shared libraries
│   ├── lib.vcxproj            # Utilities (Result, Logger, Errors)
│   ├── network.vcxproj        # IOCP async I/O library
│   └── protocol.vcxproj       # FlatBuffers protocol definitions
├── /exec/echo/                 # Echo server implementation
│   ├── echo-server.vcxproj    # Echo server executable
│   └── echo-client.vcxproj    # Test client executable
├── /exec/chat/                 # Chat server implementation
│   ├── chat-server.vcxproj    # Chat server executable
│   └── chat-client.vcxproj    # Chat client executable
├── /exec/
│   └── playground.vcxproj     # Development playground
└── /scripts/                   # Build and config scripts
```

### Project Dependencies

```
echo-server.exe
  └─> network (static lib)
       └─> lib (static lib)

echo-client.exe
  └─> network (static lib)
       └─> lib (static lib)

chat-server.exe
  └─> network (static lib)
  └─> protocol (static lib)
       └─> lib (static lib)
```

## Prerequisites and Setup

### Required Software

1. **Visual Studio 2022** (Community, Professional, or Enterprise)
   - Workload: "Desktop development with C++"
   - Components: C++ core features, MSVC v143, Windows SDK

2. **PowerShell 5.1+** (included with Windows)
   - Required for configuration script execution

3. **FlatBuffers Compiler** (optional, for protocol changes)
   - Download from: https://github.com/google/flatbuffers/releases
   - Add `flatc.exe` to PATH

### Environment Setup

1. **Clone the repository**:
   ```powershell
   git clone <repository-url>
   cd highp-mmorpg
   ```

2. **Verify directory structure**:
   ```powershell
   # Expected directories
   ls network, lib, exec, scripts
   ```

3. **Generate initial configuration headers**:
   ```powershell
   powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
   powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1
   ```

4. **Open the solution**:
   ```powershell
   start highp-mmorpg.slnx
   ```

## Build Commands

### GUI Build (Visual Studio)

1. **Open Solution**:
   ```powershell
   start highp-mmorpg.slnx
   ```

2. **Select Configuration**:
   - Toolbar: Set configuration (Debug/Release)
   - Toolbar: Set platform (x64/x86)

3. **Build**:
   - **Build Solution**: `Ctrl+Shift+B` or Build > Build Solution
   - **Build Project**: Right-click project > Build
   - **Rebuild Solution**: Build > Rebuild Solution (clean + build)

4. **Run**:
   - **Start Debugging**: `F5`
   - **Start Without Debugging**: `Ctrl+F5`

### Command-Line Build (MSBuild)

**Prerequisites**: Open "Developer Command Prompt for VS 2022" or "Developer PowerShell for VS 2022"

#### Basic Build Commands

```powershell
# Build Debug configuration (x64)
msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64

# Build Release configuration (x64)
msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64

# Rebuild (clean + build)
msbuild highp-mmorpg.slnx /t:Rebuild /p:Configuration=Debug /p:Platform=x64

# Clean only
msbuild highp-mmorpg.slnx /t:Clean /p:Configuration=Debug /p:Platform=x64
```

#### Advanced Build Options

```powershell
# Parallel build (use all CPU cores)
msbuild highp-mmorpg.slnx /m /p:Configuration=Release /p:Platform=x64

# Verbose logging
msbuild highp-mmorpg.slnx /v:detailed /p:Configuration=Debug /p:Platform=x64

# Build specific project only
msbuild exec/echo/echo-server/echo-server.vcxproj /p:Configuration=Debug /p:Platform=x64

# Suppress warnings (not recommended)
msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64 /p:WarningLevel=0
```

## Build Configurations

### Available Configurations

| Configuration | Platform | Optimization | Debug Info | Use Case |
|---------------|----------|--------------|------------|----------|
| Debug | x64 | Disabled (Od) | Full (PDB) | Development, debugging |
| Release | x64 | Maximum (O2) | Minimal | Production, performance testing |
| Debug | x86 | Disabled (Od) | Full (PDB) | Legacy 32-bit debugging |
| Release | x86 | Maximum (O2) | Minimal | Legacy 32-bit production |

### Primary Configuration: Debug x64

- **Compiler Flags**: `/Od /Zi /MDd`
- **Linker Flags**: `/DEBUG`
- **Preprocessor**: `_DEBUG`
- **Output**: `x64\Debug\<project>.exe`
- **Working Directory**: Project root

### Primary Configuration: Release x64

- **Compiler Flags**: `/O2 /MD`
- **Linker Flags**: `/OPT:REF /OPT:ICF`
- **Preprocessor**: `NDEBUG`
- **Output**: `x64\Release\<project>.exe`
- **Working Directory**: Project root

### Switching Configurations

**Visual Studio GUI**:
```
Toolbar > Configuration dropdown > Select "Debug" or "Release"
Toolbar > Platform dropdown > Select "x64" or "x86"
```

**Command Line**:
```powershell
msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64
```

## Building Individual Projects

### Network Library (Static Library)

The core async I/O library with IOCP implementation.

```powershell
# Visual Studio
Right-click "network" project > Build

# Command line
msbuild network/network.vcxproj /p:Configuration=Debug /p:Platform=x64
```

**Output**: `x64\Debug\network.lib`

**Key Components**:
- `IoCompletionPort`: IOCP wrapper and worker thread management
- `Acceptor`: AcceptEx-based async connection acceptance
- `Client`: Per-connection state and I/O operations

### Echo Server (Executable)

Server application demonstrating the network library.

```powershell
# Visual Studio
Right-click "echo-server" project > Build

# Command line
msbuild exec/echo/echo-server/echo-server.vcxproj /p:Configuration=Debug /p:Platform=x64
```

**Output**: `x64\Debug\echo-server.exe`

**Dependencies**:
- `network.lib` (automatically linked)
- `lib.lib` (automatically linked)
- `ws2_32.lib` (Windows Sockets)
- `mswsock.lib` (AcceptEx, WSARecvMsg extensions)

### Echo Client (Executable)

Test client for echo server validation.

```powershell
# Visual Studio
Right-click "echo-client" project > Build

# Command line
msbuild exec/echo/echo-client/echo-client.vcxproj /p:Configuration=Debug /p:Platform=x64
```

**Output**: `x64\Debug\echo-client.exe`

**Dependencies**:
- `network.lib`
- `lib.lib`
- `ws2_32.lib`

### Shared Utilities Library

Common utilities used across projects.

```powershell
# Command line
msbuild lib/lib.vcxproj /p:Configuration=Debug /p:Platform=x64
```

**Output**: `x64\Debug\lib.lib`

**Components**:
- `Result<T, E>`: Error handling type (similar to C++23 std::expected)
- `Logger`: Logging infrastructure
- `Errors`: Common error types

## Configuration Regeneration

The project uses TOML configuration files that are parsed into C++ headers at build time.

### Configuration Types

#### 1. Compile-Time Configuration

**File**: `network/config.compile.toml`

**Purpose**: Static buffer sizes and compile-time constants

**Output**: `network/Const.h`

**Example Configuration**:
```toml
# network/config.compile.toml
[buffer]
recv_buffer_size = 4096
send_buffer_size = 1024
address_buffer_size = 64
client_ip_buffer_size = 32
```

**Regenerate**:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
```

**Generated Output** (`network/Const.h`):
```cpp
namespace highp::network {
struct Const {
    struct Buffer {
        static constexpr INT recvBufferSize = 4096;
        static constexpr INT sendBufferSize = 1024;
        static constexpr INT addressBufferSize = 64;
        static constexpr INT clientIpBufferSize = 32;
    };
};
}
```

#### 2. Runtime Configuration

**File**: `exec/echo/echo-server/config.runtime.toml`

**Purpose**: Runtime-configurable server settings (port, thread counts, etc.)

**Output**: `network/NetworkCfg.h`

**Example Configuration**:
```toml
# exec/echo/echo-server/config.runtime.toml
[server]
port = 8080
max_clients = 1000
backlog = 10

[thread]
max_worker_thread_multiplier = 2
max_acceptor_thread_count = 2
desirable_iocp_thread_count = -1
```

**Regenerate**:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1
```

**Environment Variable Overrides**:
```powershell
# Override server port
$env:SERVER_PORT = "9090"
.\x64\Debug\echo-server.exe

# Override max clients
$env:SERVER_MAX_CLIENTS = "500"
.\x64\Debug\echo-server.exe
```

**Generated Output** (`network/NetworkCfg.h`):
```cpp
namespace highp::network {
struct NetworkCfg {
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

    static NetworkCfg FromFile(const std::filesystem::path& path);
    static NetworkCfg WithDefaults();
};
}
```

#### 3. FlatBuffers Schema Compilation

**Files**: `protocol/flatbuf/schemas/*.fbs`

**Purpose**: Protocol buffer schema compilation for client-server messaging

**Output**: `protocol/flatbuf/gen/*_generated.h`

**Regenerate**:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/gen_flatbuffers.ps1
```

**Requirements**:
- FlatBuffers compiler (`flatc.exe`) must be in PATH
- Download from: https://github.com/google/flatbuffers/releases

**Script Behavior**:
- Recursively finds all `.fbs` files in `protocol/flatbuf/schemas/`
- Compiles each with `--cpp --scoped-enums` flags
- Outputs C++ headers to `protocol/flatbuf/gen/`
- Reports success/failure for each schema

### When to Regenerate

**Compile-Time Config** (`parse_compile_cfg.ps1`):
- After modifying `network/config.compile.toml`
- Before building if buffer sizes changed
- Required before first build

**Runtime Config** (`parse_network_cfg.ps1`):
- After modifying `exec/echo/echo-server/config.runtime.toml`
- Before building if new config fields added
- Not required for value-only changes (use env vars instead)

**FlatBuffers** (`gen_flatbuffers.ps1`):
- After adding/modifying `.fbs` schema files
- When protocol definitions change
- Only if using chat server or custom protocols

### Automation

**Pre-Build Event** (recommended):
Add to project properties to auto-regenerate on build:

```xml
<PreBuildEvent>
  <Command>
    powershell -ExecutionPolicy Bypass -File "$(SolutionDir)scripts\parse_compile_cfg.ps1"
    powershell -ExecutionPolicy Bypass -File "$(SolutionDir)scripts\parse_network_cfg.ps1"
  </Command>
</PreBuildEvent>
```

## Testing Approach

### Current Testing Strategy

The project currently uses **manual integration testing** with no automated test framework.

**Testing Philosophy**:
- Focus on real-world integration scenarios
- Validate IOCP behavior under actual network conditions
- Test concurrent connection handling
- Verify echo protocol correctness

**No Unit Testing Framework**:
- No Google Test, Catch2, or similar frameworks
- No mocking or test fixtures
- Direct executable-based validation

### Manual Testing Workflow

1. **Build Test Artifacts**:
   ```powershell
   msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64
   ```

2. **Start Echo Server** (Terminal 1):
   ```powershell
   cd C:\Users\dlrnt\werk\highp-mmorpg
   .\x64\Debug\echo-server.exe
   ```

3. **Run Echo Client** (Terminal 2):
   ```powershell
   cd C:\Users\dlrnt\werk\highp-mmorpg
   .\x64\Debug\echo-client.exe
   ```

4. **Observe Behavior**:
   - Server logs connection acceptance
   - Client sends test messages
   - Server echoes messages back
   - Client validates received data
   - Both report errors/warnings

### Test Scenarios

#### Basic Connectivity
```
Test: Single client connection
Expected: Server accepts, client connects, bidirectional communication
```

#### Echo Protocol
```
Test: Send "Hello, World!" from client
Expected: Server echoes back "Hello, World!" exactly
```

#### Concurrent Connections
```
Test: Start multiple echo-client.exe instances
Expected: All clients connect and echo successfully
Max: config.runtime.toml [server] max_clients (default 1000)
```

#### Graceful Shutdown
```
Test: Ctrl+C on server
Expected: Graceful cleanup, no resource leaks
```

#### Configuration Override
```
Test: Set $env:SERVER_PORT = "9090" and restart server
Expected: Server listens on port 9090 instead of 8080
```

### Future Testing Considerations

**Automated Testing** (not currently implemented):
- Unit tests for `Result<T, E>` type
- Mock IOCP operations for network layer
- Integration test suite with scripted clients
- Performance benchmarks (throughput, latency)

**Load Testing**:
- Stress test with tools like `wrk` or custom client
- Measure max concurrent connections
- Profile CPU/memory usage under load

## Running the Echo Server and Client

### Echo Server

**Purpose**: Demonstrates IOCP-based async server with AcceptEx and WSARecv/WSASend.

#### Starting the Server

**Standard Start**:
```powershell
cd C:\Users\dlrnt\werk\highp-mmorpg
.\x64\Debug\echo-server.exe
```

**With Configuration Override**:
```powershell
# Change port
$env:SERVER_PORT = "9090"
.\x64\Debug\echo-server.exe

# Change max clients
$env:SERVER_MAX_CLIENTS = "500"
$env:SERVER_PORT = "8080"
.\x64\Debug\echo-server.exe
```

**Configuration File**:
The server reads `config.runtime.toml` from the **working directory**:
```powershell
# Ensure you run from project root
cd C:\Users\dlrnt\werk\highp-mmorpg
.\x64\Debug\echo-server.exe
```

#### Expected Output

```
[INFO] Loading configuration from config.runtime.toml
[INFO] Server configuration:
       Port: 8080
       Max Clients: 1000
       Worker Threads: 16
       Acceptor Threads: 2
[INFO] Starting IOCP worker threads...
[INFO] Starting acceptor threads...
[INFO] Echo server listening on 0.0.0.0:8080
[INFO] Press Ctrl+C to stop
```

#### Runtime Behavior

**On Client Connection**:
```
[INFO] Accepted connection from 127.0.0.1:54321
[INFO] Client count: 1/1000
```

**On Message Received**:
```
[DEBUG] Recv from 127.0.0.1:54321: 13 bytes
[DEBUG] Echo back to 127.0.0.1:54321: 13 bytes
```

**On Client Disconnect**:
```
[INFO] Client 127.0.0.1:54321 disconnected
[INFO] Client count: 0/1000
```

### Echo Client

**Purpose**: Test client that sends messages and validates echo responses.

#### Starting the Client

**Standard Start**:
```powershell
cd C:\Users\dlrnt\werk\highp-mmorpg
.\x64\Debug\echo-client.exe
```

**Connect to Custom Port**:
Modify client source or use command-line args (if implemented):
```powershell
.\x64\Debug\echo-client.exe --port 9090
```

#### Expected Output

```
[INFO] Connecting to 127.0.0.1:8080...
[INFO] Connected successfully
[INFO] Sending: "Hello, World!"
[INFO] Received: "Hello, World!"
[INFO] Echo test passed!
[INFO] Disconnecting...
```

### Multi-Client Testing

**Launch Multiple Clients**:
```powershell
# Terminal 1: Server
.\x64\Debug\echo-server.exe

# Terminal 2: Client 1
.\x64\Debug\echo-client.exe

# Terminal 3: Client 2
.\x64\Debug\echo-client.exe

# Terminal 4: Client 3
.\x64\Debug\echo-client.exe
```

**Scripted Multi-Client Launch**:
```powershell
# Launch 10 clients concurrently
1..10 | ForEach-Object {
    Start-Process -FilePath ".\x64\Debug\echo-client.exe" -NoNewWindow
}
```

### Stopping the Server

**Graceful Shutdown**:
```
Press Ctrl+C in server terminal
```

**Expected Shutdown Behavior**:
```
[INFO] Shutdown signal received
[INFO] Stopping acceptor threads...
[INFO] Closing client connections...
[INFO] Stopping worker threads...
[INFO] Cleanup complete
```

**Forceful Termination** (not recommended):
```powershell
# Task Manager > End Task
# Or via command line:
Stop-Process -Name "echo-server"
```

## Troubleshooting Common Build Issues

### Issue: "Cannot open solution file"

**Symptom**:
```
error MSB4025: The project file could not be loaded. The system cannot find the file specified.
```

**Cause**: Wrong working directory or missing solution file.

**Solution**:
```powershell
# Verify solution exists
Test-Path "C:\Users\dlrnt\werk\highp-mmorpg\highp-mmorpg.slnx"

# Navigate to project root
cd C:\Users\dlrnt\werk\highp-mmorpg

# Retry build
msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64
```

### Issue: "Missing Const.h or NetworkCfg.h"

**Symptom**:
```
fatal error C1083: Cannot open include file: 'network/Const.h': No such file or directory
```

**Cause**: Configuration headers not generated.

**Solution**:
```powershell
# Generate compile-time config
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1

# Generate runtime config
powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1

# Verify output
Test-Path network\Const.h
Test-Path network\NetworkCfg.h

# Rebuild
msbuild highp-mmorpg.slnx /t:Rebuild /p:Configuration=Debug /p:Platform=x64
```

### Issue: "MSBuild not recognized"

**Symptom**:
```
'msbuild' is not recognized as an internal or external command
```

**Cause**: Not using Developer Command Prompt.

**Solution**:
```powershell
# Option 1: Use Developer Command Prompt
Start Menu > Visual Studio 2022 > Developer Command Prompt for VS 2022

# Option 2: Manually add to PATH
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin"

# Option 3: Use full path
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" highp-mmorpg.slnx
```

### Issue: "LNK1104: cannot open file 'network.lib'"

**Symptom**:
```
LINK : fatal error LNK1104: cannot open file 'x64\Debug\network.lib'
```

**Cause**: Dependency library not built.

**Solution**:
```powershell
# Build dependencies first
msbuild network/network.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild lib/lib.vcxproj /p:Configuration=Debug /p:Platform=x64

# Then build dependent project
msbuild exec/echo/echo-server/echo-server.vcxproj /p:Configuration=Debug /p:Platform=x64

# Or rebuild entire solution (automatically resolves)
msbuild highp-mmorpg.slnx /t:Rebuild /p:Configuration=Debug /p:Platform=x64
```

### Issue: "Server fails to start with port bind error"

**Symptom**:
```
[ERROR] Failed to bind to port 8080: Address already in use
```

**Cause**: Port already occupied by another process.

**Solution**:
```powershell
# Find process using port 8080
netstat -ano | findstr :8080

# Kill the process (replace <PID> with actual process ID)
Stop-Process -Id <PID> -Force

# Or use different port
$env:SERVER_PORT = "9090"
.\x64\Debug\echo-server.exe
```

### Issue: "Execution policy prevents script execution"

**Symptom**:
```
File scripts\parse_compile_cfg.ps1 cannot be loaded because running scripts is disabled
```

**Cause**: PowerShell execution policy restriction.

**Solution**:
```powershell
# Temporary bypass (current session only)
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1

# Permanent fix (requires admin)
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser

# Then run normally
.\scripts\parse_compile_cfg.ps1
```

### Issue: "C++17/C++20 feature not supported"

**Symptom**:
```
error C7525: inline variables are only available with /std:c++17
```

**Cause**: Wrong C++ standard specified.

**Solution**:
```powershell
# Check project settings
# Visual Studio: Project Properties > C/C++ > Language > C++ Language Standard
# Should be: ISO C++17 Standard (/std:c++17) or later

# Or add to vcxproj
<LanguageStandard>stdcpp17</LanguageStandard>

# Rebuild
msbuild highp-mmorpg.slnx /t:Rebuild /p:Configuration=Debug /p:Platform=x64
```

### Issue: "FlatBuffers compiler not found"

**Symptom**:
```
Error: flatc compiler not found in PATH
```

**Cause**: FlatBuffers not installed (only needed for protocol changes).

**Solution**:
```powershell
# Option 1: Download and install FlatBuffers
# https://github.com/google/flatbuffers/releases
# Extract flatc.exe to a directory in PATH

# Option 2: Add to PATH temporarily
$env:Path += ";C:\tools\flatbuffers"

# Option 3: Skip if not using protocols
# (echo-server doesn't require FlatBuffers)
```

## CI/CD Considerations

### Continuous Integration Setup

**Recommended CI Platform**: GitHub Actions, Azure Pipelines, or Jenkins with Windows runners.

#### Sample GitHub Actions Workflow

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest

    steps:
    - name: Checkout code
      uses: actions/checkout@v3

    - name: Setup MSBuild
      uses: microsoft/setup-msbuild@v1.1

    - name: Generate configuration headers
      run: |
        powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
        powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1

    - name: Build solution (Debug)
      run: msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64 /m

    - name: Build solution (Release)
      run: msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64 /m

    - name: Upload artifacts
      uses: actions/upload-artifact@v3
      with:
        name: binaries
        path: |
          x64/Debug/*.exe
          x64/Release/*.exe
```

#### Sample Azure Pipelines Configuration

```yaml
trigger:
  - main

pool:
  vmImage: 'windows-latest'

steps:
- task: VSBuild@1
  inputs:
    solution: 'highp-mmorpg.slnx'
    platform: 'x64'
    configuration: 'Release'
    msbuildArgs: '/m'

- script: |
    powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
    powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1
  displayName: 'Generate configuration headers'

- task: PublishBuildArtifacts@1
  inputs:
    pathToPublish: 'x64/Release'
    artifactName: 'release-binaries'
```

### Build Optimization for CI

**Parallel Builds**:
```powershell
msbuild highp-mmorpg.slnx /m /p:Configuration=Release /p:Platform=x64
```

**Incremental Builds** (cache intermediate files):
```yaml
- name: Cache build artifacts
  uses: actions/cache@v3
  with:
    path: |
      x64/
      network/x64/
      lib/x64/
    key: ${{ runner.os }}-build-${{ hashFiles('**/*.cpp', '**/*.h') }}
```

**Faster Linking**:
```xml
<!-- Add to Release configuration -->
<LinkIncremental>false</LinkIncremental>
<GenerateDebugInformation>false</GenerateDebugInformation>
```

### Automated Testing in CI

**Integration Test Script** (`scripts/run_integration_tests.ps1`):
```powershell
# Start server in background
$serverProcess = Start-Process -FilePath ".\x64\Release\echo-server.exe" -PassThru -NoNewWindow

# Wait for server startup
Start-Sleep -Seconds 2

# Run client test
$clientOutput = & ".\x64\Release\echo-client.exe" 2>&1

# Kill server
Stop-Process -Id $serverProcess.Id -Force

# Validate output
if ($clientOutput -match "Echo test passed") {
    Write-Host "Integration test PASSED"
    exit 0
} else {
    Write-Host "Integration test FAILED"
    exit 1
}
```

**CI Integration**:
```yaml
- name: Run integration tests
  run: powershell -ExecutionPolicy Bypass -File scripts/run_integration_tests.ps1
```

### Deployment Considerations

**Artifact Packaging**:
```powershell
# Package for deployment
Compress-Archive -Path "x64\Release\*" -DestinationPath "release-v1.0.0.zip"
```

**Required Runtime Files**:
```
deployment/
├── echo-server.exe
├── config.runtime.toml
├── vcruntime140.dll (if not statically linked)
└── msvcp140.dll (if not statically linked)
```

**Static Linking** (eliminates runtime DLL dependencies):
```xml
<!-- Add to Release configuration -->
<RuntimeLibrary>MultiThreaded</RuntimeLibrary>
```

### Environment-Specific Configuration

**Production Config**:
```toml
# config.runtime.prod.toml
[server]
port = 8080
max_clients = 10000
backlog = 128

[thread]
max_worker_thread_multiplier = 4
max_acceptor_thread_count = 4
```

**Staging Config**:
```toml
# config.runtime.staging.toml
[server]
port = 8080
max_clients = 1000
backlog = 10
```

**CI Selection**:
```powershell
# Copy appropriate config for environment
Copy-Item "config.runtime.prod.toml" "config.runtime.toml"
```

### Versioning and Releases

**Semantic Versioning**:
```cpp
// version.h (auto-generated in CI)
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_STRING "1.0.0"
#define BUILD_NUMBER "456" // CI build number
```

**Git Tag Integration**:
```yaml
- name: Create release
  if: startsWith(github.ref, 'refs/tags/v')
  uses: actions/create-release@v1
  with:
    tag_name: ${{ github.ref }}
    release_name: Release ${{ github.ref }}
    draft: false
    prerelease: false
```

### Performance Monitoring

**Build Time Tracking**:
```yaml
- name: Build with timing
  run: |
    $start = Get-Date
    msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64 /m
    $duration = (Get-Date) - $start
    Write-Host "Build completed in $($duration.TotalSeconds) seconds"
```

**Binary Size Tracking**:
```powershell
Get-ChildItem x64\Release\*.exe | Select-Object Name, @{Name="Size(MB)";Expression={$_.Length/1MB}}
```

---

## Additional Resources

- **Project Documentation**: See `CLAUDE.md` for architecture overview
- **PowerShell Scripts**: See `scripts/` directory for automation tools
- **Visual Studio Docs**: https://docs.microsoft.com/visualstudio/
- **MSBuild Reference**: https://docs.microsoft.com/visualstudio/msbuild/

## Quick Reference

```powershell
# Full build workflow
cd C:\Users\dlrnt\werk\highp-mmorpg
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1
msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64 /m

# Run server
.\x64\Debug\echo-server.exe

# Run client (new terminal)
.\x64\Debug\echo-client.exe
```

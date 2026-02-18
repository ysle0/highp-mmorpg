# Quick Start Guide

> **Goal:** Get the echo server running in under 5 minutes

---

## 30-Second Setup

```powershell
# 1. Clone and navigate
cd C:\Users\dlrnt\werk\highp-mmorpg

# 2. Generate configs
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1

# 3. Build
msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64 /m

# 4. Run (Terminal 1)
.\x64\Debug\echo-server.exe

# 5. Test (Terminal 2)
.\x64\Debug\echo-client.exe
```

---

## Common Commands Cheat Sheet

### Build Commands
```powershell
# Debug build
msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64

# Release build
msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64

# Clean rebuild
msbuild highp-mmorpg.slnx /t:Rebuild /p:Configuration=Debug /p:Platform=x64
```

### Configuration
```powershell
# Regenerate compile-time config (after changing buffer sizes)
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1

# Regenerate runtime config (after adding config fields)
powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1

# Override server port at runtime
$env:SERVER_PORT = "9090"
.\x64\Debug\echo-server.exe
```

### Testing
```powershell
# Start server
.\x64\Debug\echo-server.exe

# Run client (new terminal)
.\x64\Debug\echo-client.exe

# Multiple clients
1..10 | ForEach-Object { Start-Process ".\x64\Debug\echo-client.exe" -NoNewWindow }
```

---

## Project Structure at a Glance

```
highp-mmorpg/
├── network/               # IOCP async I/O library
│   ├── IocpIoMultiplexer  # IOCP + worker threads
│   ├── IocpAcceptor       # AcceptEx async accept
│   ├── ServerCore         # Lifecycle management
│   └── Client             # Per-connection state
├── exec/echo/             # Echo server implementation
│   ├── echo-server/       # Server executable
│   └── echo-client/       # Test client
├── lib/                   # Utilities (Result, Logger)
├── docs/                  # You are here!
└── scripts/               # Build automation
```

---

## Key Concepts (30 seconds)

**IOCP** = Windows I/O Completion Ports (async I/O multiplexing)
**AcceptEx** = Async connection acceptance API
**Result<T, E>** = Error handling type (no exceptions)
**Compile Config** = Buffer sizes (requires rebuild)
**Runtime Config** = Server settings (env var overrides)

---

## Next Steps

| Task | Documentation |
|------|---------------|
| **Understand Architecture** | [ARCHITECTURE.md](ARCHITECTURE.md) |
| **Learn Network Layer** | [NETWORK_LAYER.md](NETWORK_LAYER.md) |
| **API Reference** | [API_REFERENCE.md](API_REFERENCE.md) |
| **Configuration Tuning** | [CONFIGURATION.md](CONFIGURATION.md) |
| **Protocol Design** | [PROTOCOL.md](PROTOCOL.md) |
| **Build Troubleshooting** | [BUILD_AND_TEST.md](BUILD_AND_TEST.md) |

---

## Common Issues (Quick Fixes)

### Build fails with "Cannot find Const.h"
```powershell
powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1
powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1
msbuild highp-mmorpg.slnx /t:Rebuild /p:Configuration=Debug /p:Platform=x64
```

### Server won't bind to port
```powershell
# Find process using port 8080
netstat -ano | findstr :8080

# Kill it (replace PID)
Stop-Process -Id <PID> -Force

# Or use different port
$env:SERVER_PORT = "9090"
.\x64\Debug\echo-server.exe
```

### MSBuild not found
```powershell
# Use Developer Command Prompt for VS 2022
# Start Menu > Visual Studio 2022 > Developer Command Prompt
```

---

## Performance Tuning Quick Reference

| Setting | File | Default | Notes |
|---------|------|---------|-------|
| Recv buffer | `config.compile.toml` | 4KB | Requires rebuild |
| Send buffer | `config.compile.toml` | 1KB | Requires rebuild |
| Server port | `config.runtime.toml` | 8080 | Use `$env:SERVER_PORT` |
| Max clients | `config.runtime.toml` | 1000 | 10KB per client |
| Worker threads | `config.runtime.toml` | 2× cores | CPU vs I/O bound |

---

**See Full Documentation:** [INDEX.md](INDEX.md)

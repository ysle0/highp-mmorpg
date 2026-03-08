<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/util

## Purpose
Network utility functions shared across the network library subsystems.

## Key Files
| File | Description |
|------|-------------|
| NetworkUtil.hpp | Network utility helpers — address formatting, error code translation, byte order conversion, and other common network operations |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- Helpers here are free functions or small structs; do not introduce stateful classes.
- Use `.hpp` extension since definitions are inline/header-only.
- Prefer these utilities over raw WSA calls in other network/ files to keep error handling consistent.

### Common Patterns
- WSAGetLastError() results are typically translated to human-readable strings via helpers in this file.
- Address-to-string conversion (`inet_ntop`) is wrapped here for consistent logging format across acceptor and client code.

## Dependencies
### Internal
- `lib/inc/logger/` — logging utilities may be used in helper implementations

### External
- Windows SDK: `winsock2.h`, `ws2tcpip.h`

<!-- MANUAL: -->

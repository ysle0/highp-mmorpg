<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc/transport

## Purpose
Network transport protocol abstraction. Defines the `ETransport` enum (TCP/UDP) and `WindowsNetworkTransport` which provides socket creation parameters per protocol. A platform typedef alias selects the active transport implementation.

## Key Files
| File | Description |
|------|-------------|
| NetworkTransport.hpp | ETransport enum (TCP/UDP); WindowsNetworkTransport maps protocol to SOCK_TYPE/IPPROTO values; platform typedef alias NetworkTransport |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- `ETransport` is used by `SocketHelper` to parameterize socket creation — pass `ETransport::TCP` for stream sockets.
- The `NetworkTransport` typedef alias resolves to `WindowsNetworkTransport` on Windows; add other platform implementations here if porting.
- This file uses `.hpp` because it contains inline implementation (enum + struct definitions used directly by headers).

### Common Patterns
- `WindowsNetworkTransport::GetSocketParams(ETransport::TCP)` returns `{AF_INET, SOCK_STREAM, IPPROTO_TCP}`.
- Consumed at socket creation time; not stored per-connection.

## Dependencies
### Internal
- `network/inc/socket/` — SocketHelper consumes ETransport for socket creation

### External
- Windows SDK: `winsock2.h` for SOCK_* and IPPROTO_* constants

<!-- MANUAL: -->

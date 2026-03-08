<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# echo

## Purpose
Echo server/client pair - the simplest example demonstrating the IOCP networking stack end-to-end. The server echoes back whatever the client sends, making it the baseline integration test for the async I/O pipeline.

## Key Files
| File | Description |
|------|-------------|

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| echo-server/ | IOCP-based async echo server executable |
| echo-client/ | Simple test client that connects and sends messages |

## For AI Agents
### Working In This Directory
- This directory is the canonical "hello world" for the network stack.
- Changes here should remain minimal; complexity belongs in `network/`.
- Build both projects together to verify end-to-end echo behavior.

### Common Patterns
- The server implements `ISessionEventReceiver`; `OnRecv` calls `PostSend` with the received buffer.
- The client connects to `127.0.0.1:8080` by default (hardcoded in `main.cpp`).

## Dependencies
### Internal
- `network/` - async I/O primitives
- `lib/` - logging, Result type

### External
- Windows SDK (Winsock2 / IOCP)

<!-- MANUAL: -->

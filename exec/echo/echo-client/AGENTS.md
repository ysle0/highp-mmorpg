<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# echo-client

## Purpose
Simple TCP test client for the echo server. Connects to the server, sends messages, and verifies that the echoed response matches what was sent. Used for manual integration testing of the IOCP networking stack.

## Key Files
| File | Description |
|------|-------------|
| EchoClient.h / EchoClient.cpp | Client implementation - connects, sends, receives |
| main.cpp | Entry point - connects to 127.0.0.1:8080 and drives the test interaction |
| echo-client.vcxproj | Visual Studio project file |

## Subdirectories
| Directory | Purpose |
|-----------|---------|

## For AI Agents
### Working In This Directory
- This is a synchronous blocking client, not an IOCP client - keep it simple.
- The server address and port (127.0.0.1:8080) are hardcoded in `main.cpp`; adjust if the server config changes.
- Used exclusively for manual testing; there is no automated test harness.

### Common Patterns
- Connect → send a message → read back the echo → assert equality → repeat or exit.
- Use standard Winsock2 (`connect` / `send` / `recv`) rather than IOCP for simplicity.

## Dependencies
### Internal
- `lib/` - logger (optional, for diagnostic output)

### External
- Windows SDK (Winsock2)

<!-- MANUAL: -->

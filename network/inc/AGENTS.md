<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# network/inc

## Purpose
Public headers for the network library, organized by subsystem. Consumers of the network static library include only from this directory.

## Key Files
| File | Description |
|------|-------------|
| platform.h | Windows platform typedefs: SocketHandle, IOCP handle types, Windows socket includes |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| acceptor/ | AcceptEx-based async connection acceptance headers |
| client/ | Per-connection client state and I/O buffer headers |
| config/ | Auto-generated configuration headers (compile-time and runtime) |
| io/ | I/O multiplexing abstractions and IOCP implementation headers |
| server/ | Server lifecycle, packet dispatcher, and session event interfaces |
| socket/ | Socket abstractions and helper headers |
| transport/ | Network transport protocol abstraction headers |
| util/ | Network utility function headers |

## For AI Agents
### Working In This Directory
- Headers here form the public API of the network library; changes affect all consumers.
- Platform-specific headers are nested under `windows/` subdirectories within each subsystem folder.
- Do not include `src/` implementation details in these headers.

### Common Patterns
- Interfaces are prefixed with `I` (e.g., `ICompletionTarget`, `ISocket`, `ISessionEventReceiver`).
- Template/inline implementations use `.hpp` extension; pure declaration headers use `.h`.

## Dependencies
### Internal
- `lib/inc/` — functional/, error/, memory/ utilities used in header signatures

### External
- `platform.h` pulls in Windows SDK headers; all other headers assume platform.h is included via pch.h

<!-- MANUAL: -->

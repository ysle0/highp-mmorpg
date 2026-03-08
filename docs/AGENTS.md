<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# docs

## Purpose
Project documentation covering architecture, build process, code style, and protocol details. Intended for human developers and AI agents that need to understand the system before making changes. `INDEX.md` is the entry point.

## Key Files
| File | Description |
|------|-------------|
| INDEX.md | Documentation navigation map - start here |
| QUICK_START.md | Fastest path to building and running the server and echo client |
| BUILD_AND_TEST.md | Full build matrix (Debug/Release, x64), test workflow, and CI notes |
| ARCHITECTURE.md | Layer boundaries, ownership model, and end-to-end data flow |
| NETWORK_LAYER.md | IOCP internals: completion port setup, worker thread pool, overlapped I/O lifecycle |
| CONFIGURATION.md | Compile-time and runtime configuration system, env-var overrides |
| CODE_STYLE.md | Naming conventions (PascalCase types, camelCase members), formatting rules |
| PROTOCOL.md | FlatBuffers message contracts, `Packet` wire format, `Payload` union usage |
| ERROR_HANDLING.md | `Result<T, E>` type patterns and error propagation conventions |
| API_REFERENCE.md | Public API documentation for core classes |
| DIAGRAMS.md | Architecture diagrams (component and data-flow) |
| UTILITIES.md | Utility library docs (`ObjectPool`, `Result`, logging) |
| EchoServer_Sequence_diagram.md | Sequence diagram for the echo server request/response flow |
| IOCP_EchoServer_Architecture.md | Deep dive into the IOCP architecture as implemented in the echo server |
| GLOSSARY.md | Definitions for project-specific and IOCP/WinSock terminology |
| SMART_POINTER_OWNERSHIP_KO.md | Smart pointer ownership guide written in Korean |

## Subdirectories
None.

## For AI Agents
### Working In This Directory
- Always consult `INDEX.md` first to find the most relevant document before reading code.
- When making code changes that affect documented behavior (configuration keys, API signatures, data flow), update the corresponding doc file in the same commit.
- `SMART_POINTER_OWNERSHIP_KO.md` is in Korean; use it when ownership questions arise even if reading via translation.
- Sequence and architecture diagrams (`EchoServer_Sequence_diagram.md`, `IOCP_EchoServer_Architecture.md`, `DIAGRAMS.md`) are the fastest way to understand async I/O flow without reading all source files.

### Common Patterns
- Documents are plain Markdown with no build step required.
- Code snippets in docs should match the actual source; if a refactor changes a signature or config key, update the relevant doc.
- `GLOSSARY.md` is the canonical definition for terms like "overlapped", "completion port", "OverlappedExt", etc.

## Dependencies
### Internal
- All source directories - docs describe the behavior of code in `network/`, `exec/`, `lib/`, and `protocol/`

### External
- None (plain Markdown, no tooling required)

<!-- MANUAL: -->

# Repository Guidelines

## Project Structure & Module Organization
- `network/`: IOCP networking core (acceptor, multiplexer, async sockets, lifecycle).
- `lib/`: shared utilities (`Result`, logger, errors, TOML helpers).
- `protocol/`: FlatBuffers schemas (`flatbuf/schemas/`) and generated headers (`flatbuf/gen/`).
- `exec/echo/` and `exec/chat/`: runnable server/client applications.
- `scripts/`: config/code generation scripts (`parse_*`, `gen_flatbuffers`).
- `docs/`: architecture, configuration, and build references.
- Build artifacts are emitted under `x64/<Configuration>/`.

## Build, Test, and Development Commands
Use Developer PowerShell/Command Prompt for VS 2022.

- `powershell -ExecutionPolicy Bypass -File scripts/parse_compile_cfg.ps1`  
  Generate compile-time constants (`network/Const.h`).
- `powershell -ExecutionPolicy Bypass -File scripts/parse_network_cfg.ps1`  
  Generate runtime config bindings (`network/NetworkCfg.h`).
- `msbuild highp-mmorpg.slnx /p:Configuration=Debug /p:Platform=x64 /m`  
  Build all projects (parallel).
- `msbuild highp-mmorpg.slnx /t:Rebuild /p:Configuration=Release /p:Platform=x64`  
  Clean rebuild.
- `.\x64\Debug\echo-server.exe` then `.\x64\Debug\echo-client.exe`  
  Manual integration smoke test.
- `powershell -ExecutionPolicy Bypass -File scripts/gen_flatbuffers.ps1`  
  Regenerate protocol headers after `.fbs` changes.

## Coding Style & Naming Conventions
- Follow `docs/CODE_STYLE.md`.
- Indentation: tabs; braces on the same line.
- Naming: `PascalCase` for types/methods, `camelCase` for locals/params, private members as `_camelCase`.
- Enums use `E` prefix (example: `EIoType`).
- File names match primary type (`IocpAcceptor.h`, `IocpAcceptor.cpp`).
- Prefer `Result<T, E>` + `GUARD(...)` for error propagation over exceptions.

## Testing Guidelines
- Current strategy is manual integration testing (no gtest/Catch2 configured).
- Minimum checks for network changes: connect, echo round-trip, multi-client run, graceful shutdown.
- Record test commands and observed behavior in PRs.
- If you add automated tests, place them with the relevant module and document execution in `docs/BUILD_AND_TEST.md`.

## Commit & Pull Request Guidelines
- Match existing history style: `feat(scope): ...`, `refactor(scope): ...`, `chore(scope): ...`, `docs: ...`, `cfg: ...`, `script: ...`.
- Keep commits small and single-purpose.
- PRs should include:
  - what changed and why,
  - linked issue/task (if applicable),
  - validation evidence (commands/log snippets),
  - note whether config or FlatBuffers generation was required.

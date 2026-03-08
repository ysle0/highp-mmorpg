<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# flatbuf

## Purpose
FlatBuffers schema definitions and auto-generated C++ headers. This directory is the source of truth for the wire protocol. Schemas live in `schemas/` and are compiled by `flatc` into the headers under `gen/`.

## Key Files
| File | Description |
|------|-------------|
| flatbuffer-gen-analysis.md | Analysis notes on the structure and patterns of the generated code |

## Subdirectories
| Directory | Purpose |
|-----------|---------|
| schemas/ | Source `.fbs` schema files defining the protocol message types |
| gen/ | Auto-generated C++ headers produced by `flatc` - do not edit manually |

## For AI Agents
### Working In This Directory
- Treat `gen/` as read-only output. All edits must be made in `schemas/`.
- After any schema change, regenerate headers:
  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts/gen_flatbuffers.ps1
  ```
- `flatbuffer-gen-analysis.md` documents non-obvious patterns in the generated code; consult it before writing serialization logic.

### Common Patterns
- Schema files use snake_case for field names; the FlatBuffers compiler maps these to camelCase accessor methods in generated C++.
- The `Payload` union in `packet.fbs` is the single extension point for adding new message types - add a new table, then add an entry to the union.

## Dependencies
### Internal
- `scripts/gen_flatbuffers.ps1` / `gen_flatbuffers.sh` - invokes `flatc` to regenerate `gen/`

### External
- FlatBuffers `flatc` compiler (must be on PATH when running generation scripts)

<!-- MANUAL: -->

<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/concurrency

## Purpose
Thread-safe concurrent data structures. Provides lock-free or low-contention primitives for passing work between threads without blocking the IOCP worker pool.

## Key Files
| File | Description |
|------|-------------|
| MPSCCommandQueue.hpp | Lock-free multi-producer single-consumer command queue. Used by PacketDispatcher to pass commands from multiple IOCP worker threads to a single logic thread without mutex contention. |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- `MPSCCommandQueue.hpp` is header-only. Do not create a matching `.cpp`.
- The MPSC guarantee is structural: only one consumer thread may call `pop`/`dequeue` at a time. Multiple producers may call `push`/`enqueue` concurrently.
- When modifying, verify memory ordering annotations (`std::memory_order_*`) are correct for the intended visibility guarantees before submitting.

### Common Patterns
- Consumers typically drain the queue in a dedicated logic-thread loop, not inside IOCP completion callbacks.
- Commands enqueued are usually small value types or `std::function` wrappers; avoid storing raw pointers without shared ownership.

## Dependencies
### Internal
- None within `lib/inc/concurrency/`.

### External
- `<atomic>` (C++ standard library) for lock-free operations.
- `<optional>` or similar for dequeue return type.

<!-- MANUAL: -->

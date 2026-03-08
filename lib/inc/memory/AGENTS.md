<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-03-08 | Updated: 2026-03-08 -->

# lib/inc/memory

## Purpose
Object pool implementations for high-performance memory reuse. Eliminates repeated heap allocation/deallocation on hot paths (e.g., overlapped I/O contexts) by recycling fixed-size objects.

## Key Files
| File | Description |
|------|-------------|
| HybridObjectPool.hpp | Hybrid pool combining a thread-local fast path with a shared fallback pool. Preferred for high-frequency allocations across multiple IOCP worker threads. |
| LockObjectPool.hpp | Mutex-based thread-safe object pool. Simpler than the hybrid pool; suitable for lower-frequency allocations or single-threaded use. |
| ThreadLocalPool.hpp | Thread-local object pool with zero contention. Objects are acquired and returned on the same thread; not safe for cross-thread return. |

## Subdirectories (if any)
None.

## For AI Agents
### Working In This Directory
- All pool headers are header-only templates. Do not create matching `.cpp` files.
- Choose the right pool for the access pattern:
  - `HybridObjectPool`: multiple IOCP worker threads acquiring and returning objects (the common case for `OverlappedExt`).
  - `LockObjectPool`: simple shared access where contention is low.
  - `ThreadLocalPool`: single-threaded hot path where objects are always returned on the same thread they were acquired from.
- Pool objects must be default-constructible or constructible from a factory lambda provided at pool creation.
- Returned objects should be reset to a clean state before being put back into the pool; the pool itself does not zero memory.

### Common Patterns
- `ObjectPool<OverlappedExt>` is the canonical usage in `network/` for I/O context recycling.
- Acquire with `pool.Acquire()` (returns a smart handle that auto-returns on destruction) rather than raw `Alloc`/`Free` to avoid leaks.

## Dependencies
### Internal
- None within `lib/inc/memory/`.

### External
- C++17 `<mutex>`, `<vector>`, `<memory>` (for `LockObjectPool`).
- Compiler thread-local storage (`thread_local`) for `ThreadLocalPool` and `HybridObjectPool`.

<!-- MANUAL: -->

# Performance Measurement Plan

This document identifies every measurable bottleneck in the codebase and defines a concrete plan for benchmarking each one, including what to measure, how to measure it, and what alternative implementations to evaluate.

---

## Table of Contents

1. [Measurement Infrastructure](#1-measurement-infrastructure)
2. [Bottleneck: MPSCCommandQueue Mutex](#2-bottleneck-mpsccommandqueue-mutex)
3. [Bottleneck: HybridObjectPool Global Mutex](#3-bottleneck-hybridobjectpool-global-mutex)
4. [Bottleneck: LockObjectPool Mutex](#4-bottleneck-lockobjectpool-mutex)
5. [Bottleneck: DeferredItemHolder Mutex](#5-bottleneck-deferreditemholder-mutex)
6. [Bottleneck: PacketCommand Vector Allocation](#6-bottleneck-packetcommand-vector-allocation)
7. [Bottleneck: SendFrame Heap Allocation](#7-bottleneck-sendframe-heap-allocation)
8. [Bottleneck: FrameBuffer memmove on Consume](#8-bottleneck-framebuffer-memmove-on-consume)
9. [Bottleneck: shared_ptr Atomic Ref Counting](#9-bottleneck-shared_ptr-atomic-ref-counting)
10. [Bottleneck: Logger stdout Flushing](#10-bottleneck-logger-stdout-flushing)
11. [Bottleneck: FindAvailableClient Linear Scan](#11-bottleneck-findavailableclient-linear-scan)
12. [Bottleneck: std::function in CompletionHandler](#12-bottleneck-stdfunction-in-completionhandler)
13. [Bottleneck: FlatBuffers Verify + Double Parse](#13-bottleneck-flatbuffers-verify--double-parse)
14. [Bottleneck: ZeroMemory on OVERLAPPED per I/O](#14-bottleneck-zeromemory-on-overlapped-per-io)
15. [Bottleneck: WSASocketW per AcceptEx](#15-bottleneck-wsasocketw-per-acceptex)
16. [Summary: Priority Matrix](#16-summary-priority-matrix)

---

## 1. Measurement Infrastructure

### Micro-benchmark Harness

Create a `bench/` directory with a lightweight harness using `<chrono>` and Windows `QueryPerformanceCounter`. Each benchmark should:

- Warm up for N iterations (discard first 1000)
- Measure P50, P95, P99, max latencies
- Report throughput (ops/sec) and average latency (ns/op)
- Run under controlled conditions: pinned threads, release build, no debugger

```
bench/
  BenchMPSCQueue.cpp
  BenchObjectPool.cpp
  BenchFrameBuffer.cpp
  BenchLogger.cpp
  BenchSharedPtr.cpp
  BenchFindClient.cpp
  BenchSendFrame.cpp
  BenchFlatBuffers.cpp
  common/
    Timer.h           // QueryPerformanceCounter wrapper
    Stats.h           // percentile calculator
```

### Integration Profiling

For full-pipeline measurements under load:

- Use the echo-client in a loop (1000+ connections, sustained packet flood)
- Instrument with ETW (Event Tracing for Windows) or Intel VTune
- Capture CPU flame graphs to confirm which bottlenecks dominate real workloads

---

## 2. Bottleneck: MPSCCommandQueue Mutex

**File:** `lib/inc/concurrency/MPSCCommandQueue.hpp`
**Hot path:** Every packet received by any worker thread calls `Push()`. Logic thread calls `DrainTo()` every tick.

### What to Measure

| Metric | Method |
|--------|--------|
| `Push()` latency under contention | N worker threads pushing concurrently, measure per-call ns |
| `DrainTo()` latency | Single consumer thread, measure time for swap + clear |
| Lock hold time distribution | Instrument mutex acquire/release timestamps |
| Contention rate | Count spin iterations or failed try_lock attempts |
| Throughput | Total ops/sec at 2, 4, 8, 16 producer threads |

### Benchmark Design

```
Scenario: N producer threads push 1M items total, 1 consumer drains in a loop.
Vary N = {1, 2, 4, 8, 16, 32}
Measure: total wall time, per-push P99 latency, consumer drain batch sizes
```

### Alternatives to Evaluate

| Alternative | Description | Expected Tradeoff |
|------------|-------------|-------------------|
| **SpinLock** | `std::atomic_flag` with `test_and_set` | Lower latency under low contention; wastes CPU under high contention |
| **SpinLock + backoff** | Exponential backoff before yield | Better than raw spinlock under sustained contention |
| **Lock-free MPSC queue** | Intrusive linked list with CAS (Michael-Scott style) | Zero contention, but per-node allocation unless pooled |
| **`SRWLock` (Windows)** | Slim Reader/Writer Lock, lighter than `std::mutex` | Drop-in replacement, typically faster on Windows |
| **Batch push** | Accumulate locally in worker, push vector in bulk | Reduces lock frequency, increases per-push latency |

### Decision Criteria

- If P99 push latency < 200ns under 8 threads: current `std::mutex` is acceptable
- If spinlock shows > 30% improvement at target thread count: switch
- If lock-free shows > 2x throughput with no allocation regression: switch

---

## 3. Bottleneck: HybridObjectPool Global Mutex

**File:** `lib/inc/memory/HybridObjectPool.hpp`
**Hot path:** `Get()` when thread-local pool is empty (line 69), `Return()` when thread-local pool overflows capacity (line 106).

### What to Measure

| Metric | Method |
|--------|--------|
| Thread-local hit rate | Count Get() calls that resolve without global lock |
| Global lock acquisition frequency | Count lock_guard constructions per second |
| `Get()` latency: local hit vs. global fetch vs. new allocation | Separate timers for each path |
| `Return()` latency: local return vs. global spill | Separate timers |
| Chunk transfer overhead | Time for vector insert + erase during chunk migration |

### Benchmark Design

```
Scenario: N threads each do 100K Get/Return cycles.
Vary: LocalMaxCapacity = {50, 100, 200, 500}
Vary: ChunkSize = {25, 50, 100}
Measure: global lock acquisitions, total throughput, P99 per-op latency
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Increase LocalMaxCapacity** | Reduce global lock frequency at cost of per-thread memory |
| **Lock-free global pool** | Treiber stack with CAS for global pool |
| **`SRWLock`** | Windows-native, lighter than std::mutex |
| **Adaptive chunk sizing** | Start with large chunks, reduce as pool stabilizes |

### Decision Criteria

- If global lock hit rate < 5% of total Get() calls: current design is fine, just tune capacities
- If global lock contention is measurable: evaluate lock-free global pool

---

## 4. Bottleneck: LockObjectPool Mutex

**File:** `lib/inc/memory/LockObjectPool.hpp`
**Usage:** General-purpose pooling with mutex on every `Acquire()` and `Release()`.

### What to Measure

| Metric | Method |
|--------|--------|
| Acquire/Release latency | Per-call timing under varying thread counts |
| Contention rate | try_lock failure counting |
| Pool exhaustion frequency | Count fallback `make_shared<T>()` allocations |

### Benchmark Design

```
Scenario: N threads each do 100K Acquire/Release cycles from pre-allocated pool.
Vary N = {1, 2, 4, 8, 16}
Compare: LockObjectPool vs HybridObjectPool vs ThreadLocalPool
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Replace with HybridObjectPool** | Already exists; thread-local fast path eliminates most locking |
| **SpinLock** | If contention is brief and pool ops are fast |
| **Sharded pool** | N pools, thread-affinity based selection |

### Decision Criteria

- If this pool is used on hot paths: migrate to HybridObjectPool
- If only used at startup/shutdown: leave as-is (not worth optimizing)

---

## 5. Bottleneck: DeferredItemHolder Mutex

**File:** `lib/inc/scope/DeferredItemHolder.hpp`
**Usage:** Tracks active `AcceptOverlapped` objects. Locked on every `Hold()`, `Release()`, and `ForEach()`.

### What to Measure

| Metric | Method |
|--------|--------|
| Hold/Release frequency | Count calls per second under load |
| Lock hold time | Timestamp around scoped_lock |
| `unordered_map` operation cost | Time for emplace/find/erase inside lock |

### Benchmark Design

```
Scenario: Simulate accept storm - N concurrent accepts completing.
Measure: Hold + Release pair latency, map operation overhead
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **`SRWLock` (read-heavy)** | If ForEach is called more than Hold/Release |
| **Concurrent hash map** | Lock-free or sharded map (e.g., `folly::ConcurrentHashMap`) |
| **Accept-specific redesign** | Since accept rate << recv rate, may not need optimization |

### Decision Criteria

- Accept operations are low-frequency relative to recv/send
- Only optimize if profiling shows this as a measurable contributor (unlikely)

---

## 6. Bottleneck: PacketCommand Vector Allocation

**File:** `network/src/server/PacketDispatcher.cpp:100-102`
**Hot path:** Every received packet creates a `std::vector<uint8_t>` copy of the payload in `PushCommand()`.

### What to Measure

| Metric | Method |
|--------|--------|
| Allocation size distribution | Histogram of payload sizes |
| Allocation latency | Time for vector construction per packet |
| Memory allocator pressure | Track heap allocation count (ETW or custom allocator) |
| Impact on throughput | Compare packets/sec with vs. without copy |

### Benchmark Design

```
Scenario: Push 1M PacketCommands with varying payload sizes {64, 256, 1024, 4096} bytes
Measure: total wall time, per-push allocation latency
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Small buffer optimization** | `std::array<uint8_t, 256>` + size field for small packets; fall back to vector for large |
| **Arena/pool allocator** | Per-tick arena that bulk-frees after Tick() completes |
| **Zero-copy with FrameBuffer ownership** | Transfer FrameBuffer slice ownership instead of copying |
| **`pmr::vector` with pool resource** | Use polymorphic allocator with monotonic buffer |

### Decision Criteria

- If > 90% of payloads are < 256 bytes: SBO will eliminate most heap allocations
- If allocation latency > 100ns per push: invest in arena allocator

---

## 7. Bottleneck: SendFrame Heap Allocation

**File:** `network/src/client/PacketStream.cpp:17`
**Hot path:** Every `SendFrame()` call allocates `std::vector<char> frame(kHeaderSize + payload.size())`.

### What to Measure

| Metric | Method |
|--------|--------|
| SendFrame call frequency | Count per second under load |
| Allocation latency | Time for vector construction + memcpy |
| Allocation size distribution | Histogram of frame sizes |

### Benchmark Design

```
Scenario: 10K clients each sending 100 echo responses.
Measure: total SendFrame time, per-call breakdown (alloc vs memcpy vs WSASend)
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Stack buffer** | `char buf[Const::Buffer::sendBufferSize]` if max frame fits |
| **Thread-local reusable buffer** | `thread_local std::vector<char>` that grows but never shrinks |
| **Write directly to send overlapped buffer** | Compose header+payload directly in `SendOverlapped.buf` |

### Decision Criteria

- If max frame size <= send buffer constant (1024): use stack buffer
- Direct composition into SendOverlapped.buf eliminates both allocation and extra memcpy - highest priority

---

## 8. Bottleneck: FrameBuffer memmove on Consume

**File:** `network/src/client/FrameBuffer.cpp:33-36`
**Hot path:** Called after every complete frame is extracted. Shifts remaining bytes to buffer front.

### What to Measure

| Metric | Method |
|--------|--------|
| Consume call frequency | Count per recv completion |
| Bytes moved per Consume | Average and max memmove size |
| Consume latency | Time per call |
| Multi-frame ratio | How often > 1 frame is consumed per recv (chained Consume calls) |

### Benchmark Design

```
Scenario: Fill buffer with N fragmented frames, measure consume chain performance.
Vary: frame sizes, fragmentation patterns
Measure: total memmove bytes, wall time
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Ring buffer** | Read/write cursors, no memmove needed. Wrap-around with modular arithmetic |
| **Cursor-based consumption** | Track read offset, only compact when offset > threshold (e.g., half buffer) |
| **Scatter-gather** | Track frame boundaries without moving data |

### Decision Criteria

- If average memmove size > 1KB per Consume: ring buffer is worth the complexity
- If most Consume calls move < 100 bytes: current design is acceptable
- Cursor-based is a low-effort improvement regardless

---

## 9. Bottleneck: shared_ptr Atomic Ref Counting

**File:** `network/inc/client/windows/Client.h:21`, `network/inc/server/PacketDispatcher.hpp:22`
**Hot path:** Every I/O completion copies `shared_ptr<Client>`. PacketCommand stores a copy. Callbacks pass by value.

### What to Measure

| Metric | Method |
|--------|--------|
| Ref count operations per packet | Trace atomic inc/dec per packet lifecycle |
| Cache line contention | Monitor L1 cache miss rate on client ref count (VTune) |
| Copy vs move ratio | Audit all shared_ptr usage sites |
| Impact of `shared_from_this()` | Measure cost of shared_from_this vs passing existing ptr |

### Benchmark Design

```
Scenario: Simulate packet flow: GQCS → OnCompletion → shared_from_this → handler → PacketCommand → Tick → dispatch
Count: total atomic operations per packet round-trip
Measure: compare with intrusive_ptr alternative
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Pass `shared_ptr` by const ref** where possible | Eliminate unnecessary copies in callbacks |
| **`intrusive_ptr`** | Manual ref count, avoids separate control block allocation |
| **Raw pointer + lifetime guarantee** | If Client lifetime is managed by pool, raw ptr in callbacks is safe |
| **`shared_ptr<Client>` pool with reset** | Reuse shared_ptr objects to amortize control block allocation |

### Audit Checklist

- [ ] `ServerLifeCycle::HandleRecv()` line 159: `client->shared_from_this()` — can we pass the existing shared_ptr from pool?
- [ ] `PacketDispatcher::Receive()` line 13: takes `shared_ptr<Client>` by value — should be `const&`?
- [ ] `PacketDispatcher::PushCommand()` line 92: copies shared_ptr into PacketCommand — necessary for cross-thread safety
- [ ] `PacketDispatcher::DispatchPacket()` line 108: takes by value — should be `const&` or `std::move`?

### Decision Criteria

- First pass: audit and convert unnecessary copies to `const&` or `std::move` (free performance)
- If atomic ops still dominate: evaluate intrusive_ptr
- If Client lifetime is strictly pool-managed: consider raw pointer with explicit lifetime contract

---

## 10. Bottleneck: Logger stdout Flushing

**File:** `lib/src/logger/TextLogger.cpp:5-23`
**Hot path:** Every log call uses `std::cout << ... << std::endl`, which flushes the stream buffer.

### What to Measure

| Metric | Method |
|--------|--------|
| Log calls per second on hot path | Count Info/Debug calls in OnRecv, OnSend, OnAccept |
| Per-log latency | Time single Info() call including flush |
| Impact of removing hot-path logs | Compare throughput with/without logging |

### Benchmark Design

```
Scenario A: 10K packets/sec with logging enabled → measure throughput
Scenario B: 10K packets/sec with logging disabled → measure throughput
Delta = logging overhead
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Replace `std::endl` with `'\n'`** | Eliminates per-line flush; OS flushes periodically |
| **Async logging** | Queue log messages, flush in background thread |
| **Log level gating** | Skip string formatting entirely if level is filtered |
| **Compile-time log removal** | `constexpr` log level removes hot-path logs in release builds |
| **Ring buffer logger** | Fixed-size circular buffer, background writer |

### Decision Criteria

- `std::endl` → `'\n'` is a zero-cost fix, do it immediately
- Async logging is the standard solution for game servers — high priority
- Compile-time gating eliminates formatting overhead entirely in release

---

## 11. Bottleneck: FindAvailableClient Linear Scan

**File:** `network/src/ServerLifecycle.cpp:194-203`
**Hot path:** Called on every new connection. Scans entire `_clientPool` vector with `std::find_if`.

### What to Measure

| Metric | Method |
|--------|--------|
| Scan time vs pool size | Time FindAvailableClient with maxClients = {100, 1000, 5000, 10000} |
| Average scan depth | How many clients are checked before finding an available one |
| Frequency | Accept rate under load |

### Benchmark Design

```
Scenario: Pre-fill pool with N-1 active clients, measure time to find the 1 available.
Vary N = {100, 1000, 5000, 10000}
Worst case: available client is at the end
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Free list (stack/queue)** | Maintain `std::stack<int>` of available indices. O(1) acquire/release |
| **Bitmap** | Bitset tracking available slots, use `_BitScanForward` intrinsic |
| **Index queue** | `std::queue<size_t>` of free slot indices |

### Decision Criteria

- Any O(1) lookup is strictly better than O(N) scan
- Free list stack is simplest — implement this regardless of profiling results

---

## 12. Bottleneck: std::function in CompletionHandler

**File:** `network/src/io/windows/IocpIoMultiplexer.cpp:129-131`
**Hot path:** Called for every IOCP completion event. `std::function` has virtual dispatch + possible heap allocation.

### What to Measure

| Metric | Method |
|--------|--------|
| `std::function` invoke overhead | Compare with raw function pointer and template callback |
| Heap allocation | Check if handler exceeds SBO threshold (typically 16-32 bytes) |
| Call frequency | One invocation per GQCS wakeup |

### Benchmark Design

```
Scenario: 1M function invocations comparing:
  - std::function<void(CompletionEvent)>
  - void(*)(CompletionEvent)
  - template<typename F> with F stored as member
Measure: per-call overhead (ns)
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Virtual method (interface)** | `ICompletionHandler::OnCompletion()` — single vtable dispatch |
| **Template parameter** | `IocpIoMultiplexer<Handler>` — zero overhead, compile-time binding |
| **Function pointer** | Simplest, but loses captures |

### Decision Criteria

- If `std::function` invocation < 5ns per call: not worth changing
- If template approach shows measurable improvement: refactor
- `std::bind_front` at line 28 of ServerLifecycle.cpp likely fits in SBO — verify

---

## 13. Bottleneck: FlatBuffers Verify + Double Parse

**File:** `network/src/server/PacketDispatcher.cpp:122-142` (ParsePacket) and `73-88` (Tick re-parse)

**Hot path:** Every packet is verified + parsed in `Receive()`, then the raw bytes are copied into `PacketCommand.data`, and re-parsed in `Tick()` via `GetPacket()`.

### What to Measure

| Metric | Method |
|--------|--------|
| Verify latency per packet | Time `VerifyPacketBuffer()` |
| Parse latency per packet | Time `GetPacket()` |
| Double-parse overhead | Total time for verify+parse in Receive + re-parse in Tick |
| Verify as % of total recv processing | Profile under load |

### Benchmark Design

```
Scenario: 1M packets of varying complexity.
Measure:
  - VerifyPacketBuffer alone
  - GetPacket alone
  - Combined verify + parse + copy + re-parse pipeline
Compare with: verify once, pass parsed result
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Store parsed result in PacketCommand** | Avoid re-parsing in Tick(). Requires FlatBuffer object lifetime management |
| **Skip verification for trusted internal traffic** | Conditional verify based on source |
| **Verify in Tick() only (single-threaded)** | Move verification to consumer side to avoid contention |
| **Batch verify** | Amortize verifier setup cost over multiple packets |

### Decision Criteria

- If verify takes > 50% of per-packet processing time: consider conditional verification
- Double-parse is architecturally wasteful — store parsed result or payload type in command

---

## 14. Bottleneck: ZeroMemory on OVERLAPPED per I/O

**File:** `network/src/client/windows/Client.cpp:16, 40`
**Hot path:** Called on every `PostRecv()` and `PostSend()`.

### What to Measure

| Metric | Method |
|--------|--------|
| ZeroMemory latency | Time for zeroing WSAOVERLAPPED struct (~48 bytes) |
| Frequency | Once per PostRecv + once per PostSend = 2x per echo round-trip |

### Benchmark Design

```
Scenario: 1M ZeroMemory calls on 48-byte struct
Compare: ZeroMemory vs memset vs manual field reset (only Internal/InternalHigh/Offset/OffsetHigh/hEvent)
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Zero only required fields** | OVERLAPPED has 5 fields; only some need reset per reuse |
| **Pool pre-zeroed objects** | Return zeroed overlapped from pool |
| **Accept as negligible** | 48-byte memset is likely < 5ns |

### Decision Criteria

- This is almost certainly negligible (< 5ns per call)
- Only optimize if profiling shows it in a hot loop. Low priority.

---

## 15. Bottleneck: WSASocketW per AcceptEx

**File:** `network/src/acceptor/IocpAcceptor.cpp:86-92`
**Hot path:** Called once per pending accept slot.

### What to Measure

| Metric | Method |
|--------|--------|
| WSASocketW creation latency | Time per call |
| Frequency | Only when new accept slots are posted (after connection completes) |

### Benchmark Design

```
Scenario: Create 10K sockets, measure total time.
Compare: WSASocketW vs socket pool (pre-create + DisconnectEx for reuse)
```

### Alternatives to Evaluate

| Alternative | Description |
|------------|-------------|
| **Socket pool with DisconnectEx** | Reuse accepted sockets after disconnect via `TransmitFile` TF_REUSE_SOCKET |
| **Pre-created socket pool** | Create sockets upfront, assign to AcceptEx |
| **Accept as infrequent** | Accept rate is much lower than recv/send rate |

### Decision Criteria

- Accept rate is typically orders of magnitude lower than packet rate
- Only optimize if supporting > 1000 connections/sec

---

## 16. Summary: Priority Matrix

### Tier 1 — High Impact, Measure First

| # | Bottleneck | Location | Why |
|---|-----------|----------|-----|
| 2 | MPSCCommandQueue mutex | `MPSCCommandQueue.hpp:19,28` | Every packet touches this lock from N worker threads |
| 6 | PacketCommand vector alloc | `PacketDispatcher.cpp:100` | Heap allocation per packet in hot path |
| 7 | SendFrame heap alloc | `PacketStream.cpp:17` | Heap allocation per send |
| 10 | Logger stdout flush | `TextLogger.cpp:5-23` | `std::endl` flush is extremely expensive on hot path |
| 11 | FindAvailableClient scan | `ServerLifecycle.cpp:194` | O(N) scan is algorithmically wrong — fix regardless |
| 13 | FlatBuffers double parse | `PacketDispatcher.cpp` | Redundant work per packet |

### Tier 2 — Medium Impact, Measure After Tier 1

| # | Bottleneck | Location | Why |
|---|-----------|----------|-----|
| 3 | HybridObjectPool global lock | `HybridObjectPool.hpp:69,106` | Only hit when thread-local pool is empty/full |
| 8 | FrameBuffer memmove | `FrameBuffer.cpp:33` | Data movement on every frame consume |
| 9 | shared_ptr ref counting | Throughout | Atomic ops on every callback |
| 12 | std::function in IOCP loop | `IocpIoMultiplexer.cpp:129` | Virtual dispatch per completion |

### Tier 3 — Low Impact, Measure Last

| # | Bottleneck | Location | Why |
|---|-----------|----------|-----|
| 4 | LockObjectPool mutex | `LockObjectPool.hpp` | Depends on usage context |
| 5 | DeferredItemHolder mutex | `DeferredItemHolder.hpp` | Accept-rate only |
| 14 | ZeroMemory OVERLAPPED | `Client.cpp:16,40` | 48 bytes, likely < 5ns |
| 15 | WSASocketW per accept | `IocpAcceptor.cpp:86` | Low frequency |

### Quick Wins (No Measurement Needed)

These changes are clearly beneficial with no tradeoffs:

1. **`std::endl` → `'\n'`** in TextLogger.cpp — eliminates flush per log call
2. **FindAvailableClient → free list** — O(1) vs O(N)
3. **Pass `shared_ptr<Client>` by `const&`** where ownership isn't transferred
4. **Add compile-time log level gating** — skip formatting in release builds

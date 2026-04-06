# Resource / Lifetime Audit Plan — 2026-04-06

## Requirements Summary
- Analyze the codebase with emphasis on:
  - resource release correctness
  - memory leak risk
  - ownership / lifetime anomalies
- Prioritize networking lifetime, IOCP / overlapped I/O, socket / handle cleanup, and application-layer retention paths.
- Deliver evidence with exact file:line references and recommended fix order.

## Scope
- `network/` IOCP, acceptor, client, socket lifecycle
- `exec/chat/` application-layer ownership graph (session/user/room)
- `exec/echo/` recv/send lifetime interactions
- `lib/` supporting lifetime primitives where directly relevant

## Acceptance Criteria
1. Every major claim cites concrete file:line evidence.
2. Findings are classified as `confirmed` or `suspicious`.
3. Findings are prioritized by operational risk.
4. The report includes verification steps for future fixes.

## Key Findings

### 1) CONFIRMED / Critical — chat disconnect path leaks `User -> Session -> Client`, and the leaked `Client` can be recycled for a new connection
**Evidence**
- `GameLoop::Disconnect()` removes the room membership and session, but never removes the `User`: `exec/chat/chat-server/chat-server/GameLoop.cpp:128-137`
- `UserManager` stores strong ownership in `_users`: `exec/chat/chat-server/chat-server/UserManager.h:38-42`
- `UserManager::CreateUser()` inserts `shared_ptr<User>` into `_users`: `exec/chat/chat-server/chat-server/UserManager.cpp:31-40`
- `User` owns a `shared_ptr<Session>`: `exec/chat/chat-server/chat-server/User.h:30-34`, `exec/chat/chat-server/chat-server/User.cpp:5-13`
- `Session` owns a `shared_ptr<Client>`: `exec/chat/chat-server/chat-server/Session.h:21-24`, `exec/chat/chat-server/chat-server/Session.cpp:10-16`
- A disconnected `Client` becomes reusable as soon as its socket is invalidated: `network/src/client/windows/Client.cpp:127-150`
- `ServerLifeCycle::FindAvailableClient()` reuses any pooled `Client` whose `socket == INVALID_SOCKET`: `network/src/ServerLifecycle.cpp:268-276`

**Why this is risky**
- After disconnect, the `User` map can keep the old `Client` object alive.
- The same `Client` object can then be reassigned to a new accepted socket.
- Stale `User` / `Session` objects may now alias a different live connection.
- This is both a memory-retention bug and an ownership corruption bug.

**Impact**
- stale session/user state
- leaked objects across disconnects
- possible cross-session misrouting / privilege confusion if stale objects are reused indirectly

---

### 2) CONFIRMED / Critical — server shutdown does not close connected clients before destroying the network lifecycle
**Evidence**
- Chat server stops lifecycle before stopping the game loop: `exec/chat/chat-server/chat-server/Server.cpp:49-61`
- `ServerLifeCycle::Stop()` only shuts down acceptor/IOCP and clears the pool; it does **not** iterate connected clients and close them: `network/src/ServerLifecycle.cpp:126-139`
- Application-layer code can still hold `shared_ptr<Client>` through `User` / `Session`: see finding #1 evidence

**Why this is risky**
- If application-layer references still exist, clearing `_clientPool` does not destroy the `Client`.
- Sockets can remain open after the network lifecycle is torn down.
- Pending callbacks installed by `ServerLifeCycle` can outlive the lifecycle object.

**Impact**
- socket/resource leakage on shutdown
- shutdown ordering hazards
- dangling callback target risk (see finding #5)

---

### 3) CONFIRMED / Critical — canceled `AcceptEx` requests can leak accept sockets; shutdown order also creates a stale-overlapped race window
**Evidence**
- `PostAccept()` creates a fresh accept socket and stores it in `AcceptOverlapped::clientSocket`: `network/src/acceptor/IocpAcceptor.cpp:93-150`, `network/inc/client/windows/OverlappedExt.h:51-57`
- The held overlapped object is stored through `DeferredItemHolder<AcceptOverlapped>`: `network/inc/acceptor/IocpAcceptor.h:148-149`, `lib/inc/scope/DeferredItemHolder.hpp:9-27`
- The `DeferContext` used by `HybridObjectPool` only returns the overlapped object to the pool; it does not automatically close `clientSocket`: `lib/inc/scope/DeferContext.hpp:15-31`, `lib/inc/memory/HybridObjectPool.hpp:53-95`
- `IocpAcceptor::Shutdown()` only calls `CancelIoEx()` for held accepts and nulls function pointers: `network/src/acceptor/IocpAcceptor.cpp:241-254`
- `ServerLifeCycle::Stop()` destroys the acceptor **before** shutting down IOCP workers: `network/src/ServerLifecycle.cpp:126-134`

**Why this is risky**
- If a canceled accept never runs the normal completion path, the per-accept socket stored in `AcceptOverlapped::clientSocket` has no guaranteed close path.
- Destroying the acceptor before IOCP shutdown means canceled completions can still surface after the holder/object is gone.

**Impact**
- leaked accepted-socket handles during shutdown
- possible stale or freed overlapped memory being observed by workers during shutdown

**Assessment**
- socket leak: **confirmed**
- shutdown-time stale-overlapped / UAF-style race: **high-confidence suspicious** based on ordering and lifetime model

---

### 4) CONFIRMED / High — echo server double-posts `WSARecv` on the same `Client`
**Evidence**
- `ServerLifeCycle::HandleRecv()` already reposts recv after the app callback: `network/src/ServerLifecycle.cpp:240-248`
- Echo server `OnRecv()` reposts recv again: `exec/echo/echo-server/Server.cpp:51-60`
- Each `Client` has only one recv overlapped/buffer: `network/inc/client/windows/Client.h:91-95`, `network/src/client/windows/Client.cpp:73-96`

**Why this is risky**
- Reusing the same overlapped state/buffer while a previous `WSARecv` is still pending breaks ownership/lifetime assumptions for the in-flight operation.
- This is not just a logic bug; it is an I/O state ownership violation.

**Impact**
- corrupted recv state
- undefined completion behavior
- hard-to-reproduce crashes or malformed traffic handling

---

### 5) CONFIRMED / High — pooled `Client` callbacks capture raw `this` from `ServerLifeCycle`
**Evidence**
- `BuildClientCallbacks()` installs lambdas capturing `[this]`: `network/src/ServerLifecycle.cpp:19-55`
- Those callbacks are copied into every pooled `Client`: `network/src/ServerLifecycle.cpp:66-70`, `network/src/ServerLifecycle.cpp:111-114`
- Chat app can keep `Client` alive beyond lifecycle teardown: see findings #1 and #2

**Why this is risky**
- If a leaked `Client` later emits connection/disconnection/send/recv-post events, the callback target may already be destroyed.
- This is a dangling ownership edge from pooled clients back into the destroyed lifecycle object.

**Impact**
- use-after-free style callback hazards after partial shutdown or retained clients

---

### 6) SUSPICIOUS / Medium — a single send buffer is reused without explicit in-flight send ownership control
**Evidence**
- Each `Client` owns only one `sendOverlapped` and one `sendOverlapped.buf`: `network/inc/client/windows/Client.h:92-95`
- `Client::PostSend()` overwrites that same buffer and immediately posts `WSASend()`, with no pending-send guard or send queue: `network/src/client/windows/Client.cpp:99-125`
- Application code can issue sends from higher layers (`Session::Send`, room broadcast loops): `exec/chat/chat-server/chat-server/Session.cpp:38-49`, `exec/chat/chat-server/chat-server/room/Room.cpp:30-34`, `exec/chat/chat-server/chat-server/room/Room.cpp:61-67`, `exec/chat/chat-server/chat-server/room/Room.cpp:77-83`

**Why this is risky**
- If two sends overlap before the first completion is observed, the second call can overwrite the first in-flight buffer.
- The current code shows no per-client send serialization contract in the networking layer.

**Assessment**
- ownership hazard is real in code shape
- exact production impact depends on whether higher layers truly serialize all sends per client

---

### 7) SUSPICIOUS / Low-to-Medium — `HybridObjectPool` behaves like process-lifetime retention unless threads fully unwind
**Evidence**
- The pool uses thread-local and global free lists and intentionally does not provide a real global clear path: `lib/inc/memory/HybridObjectPool.hpp:20-47`, `lib/inc/memory/HybridObjectPool.hpp:116-122`

**Why this is risky**
- This is not necessarily a bug for a long-lived server, but memory retained in the pool will look leak-like in leak detectors or long-running test processes.
- It becomes more problematic when pooled objects also carry non-memory resources that are not fully reset (see finding #3).

## Risk Summary
- **Critical**: findings 1, 2, 3
- **High**: findings 4, 5
- **Medium**: finding 6
- **Low/Medium**: finding 7

## Recommended Fix Order
1. **Fix disconnect ownership release in chat server**
   - Ensure disconnect removes `User` as well as `Session`.
   - Audit whether cleanup must happen on the logic thread to avoid introducing new races.
2. **Make `ServerLifeCycle::Stop()` explicitly close/drain active clients before IOCP teardown**
   - Close sockets first, then shut down IOCP after completions are drained/canceled.
3. **Repair accept shutdown semantics**
   - Attach accept-socket cleanup to the overlapped lifetime itself or close all held accept sockets during shutdown.
   - Do not destroy acceptor-owned overlapped state until canceled completions are fully drained.
4. **Remove the extra `PostRecv()` from echo server**
5. **Add explicit per-client send serialization contract**
   - queue or guard in-flight sends in the network layer
6. **Revisit callback ownership**
   - avoid raw `this` capture into pooled clients or reset callbacks before lifecycle destruction

## Verification Steps for Future Fixes
1. Reconnect/disconnect the same chat client repeatedly and verify:
   - `UserManager::GetUserCount()` returns to baseline
   - `SessionManager` returns to baseline
   - connected-client metrics return to baseline
2. Stop the chat server with active clients attached and verify:
   - all client sockets are closed before lifecycle teardown completes
   - no callbacks fire against destroyed lifecycle objects
3. Add a shutdown stress test around pending `AcceptEx` requests and verify:
   - no leaked socket handles
   - no access to freed/stale overlapped memory
4. Run echo server recv-path test and ensure exactly one recv is outstanding per client.
5. Add a multi-send stress scenario per client to verify send buffer ownership.

## ADR / Decision Guidance
- **Decision**: treat chat disconnect cleanup and acceptor shutdown ordering as the first blockers.
- **Drivers**: correctness of ownership graph, socket/handle release, shutdown safety.
- **Alternatives considered**:
  - patch only `UserManager` cleanup first
  - patch only `ServerLifeCycle::Stop()` first
  - patch accept shutdown first
- **Why chosen**: findings #1 and #2 create live object retention during normal runtime and shutdown; finding #3 compounds shutdown failure and resource leakage.
- **Consequences**: fixes will likely require thread-affinity decisions (logic thread vs IOCP thread cleanup), not just local line edits.
- **Follow-ups**: add regression tests or at minimum deterministic shutdown harnesses before refactoring lifetime code.

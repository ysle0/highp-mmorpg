# Resource / Lifetime Fix Plan — 2026-04-06

## Requirements Summary
- Turn the previous resource/lifetime audit into a concrete fix plan.
- Explain each finding in study-friendly form:
  - what the bug is
  - what the real root cause is
  - why it happens in this codebase
  - what should be changed
  - what risks exist while fixing it
- Cover findings 1-7 from `.omx/plans/resource-lifetime-audit-2026-04-06.md`.

## Acceptance Criteria
1. Each finding has exact file:line evidence.
2. Each finding explains the ownership graph or lifecycle sequence that causes the bug.
3. Each finding includes a concrete remediation direction.
4. The plan includes implementation order and verification ideas.

## Overall Fix Strategy
Fix in this order:

1. **Normal disconnect ownership cleanup first**
   - Reason: if the ownership graph is wrong during normal runtime, shutdown fixes alone are incomplete.
2. **Forced-close / invalid-packet cleanup path second**
   - Reason: abnormal disconnect must converge to the same cleanup path as normal disconnect.
3. **Server shutdown drain/close semantics third**
   - Reason: once both normal and abnormal disconnect logic are correct, shutdown can reuse the same cleanup model.
4. **AcceptEx shutdown semantics fourth**
   - Reason: accept-side leaks/races are mostly shutdown-specific and easier to fix once server stop order is clarified.
5. **Recv/send in-flight ownership guards last**
   - Reason: they are correctness-critical, but structurally more isolated than the global ownership bugs above.

---

## 1) Chat disconnect 후 `User -> Session -> Client`가 남는 문제

### Evidence
- `GameLoop::Disconnect()` only removes room membership and session: `exec/chat/chat-server/chat-server/GameLoop.cpp:128-137`
- `UserManager` keeps strong ownership in `_users`: `exec/chat/chat-server/chat-server/UserManager.h:38-42`
- `UserManager::CreateUser()` inserts a strong `shared_ptr<User>` into `_users`: `exec/chat/chat-server/chat-server/UserManager.cpp:31-40`
- `User` strongly owns `Session`: `exec/chat/chat-server/chat-server/User.h:30-34`
- `Session` strongly owns `Client`: `exec/chat/chat-server/chat-server/Session.h:21-24`
- A pooled `Client` is considered reusable as soon as `socket == INVALID_SOCKET`: `network/src/client/windows/Client.cpp:127-150`, `network/src/ServerLifecycle.cpp:268-276`

### Root Cause
The cleanup graph is incomplete.

Disconnect currently does this:

`Client disconnect -> GameLoop::Disconnect() -> remove from room -> remove session`

But it does **not** do this:

`remove user from UserManager`

That leaves the following strong-reference chain alive:

`UserManager::_users -> User -> Session -> Client`

So even after the socket is closed, the `Client` object itself is still alive. Because pooled reuse is based only on `socket == INVALID_SOCKET`, the same `Client` object can later be reused for a different connection while stale app-layer objects still point to it.

### Why This Happens
The networking layer treats a disconnected `Client` as reusable by socket state alone, while the application layer still has strong ownership of that same object through `User` and `Session`.

So the bug is not “just a leak”; it is a **lifetime-model mismatch**:

- network layer says: “this client slot is free”
- app layer says: “this client object is still mine”

### Fix Direction
Make disconnect cleanup remove **all** app-layer owners of the connection.

Recommended change:
1. In `GameLoop::Disconnect()`, resolve the `User` by client first.
2. Remove user from its room.
3. Remove user from `UserManager`.
4. Remove session from `SessionManager`.
5. Ensure this sequence runs in exactly one place.

### Affected Files
- `exec/chat/chat-server/chat-server/GameLoop.cpp`
- `exec/chat/chat-server/chat-server/UserManager.cpp`
- potentially `Room.cpp` / `RoomManager.cpp` if removal responsibilities are centralized differently

### Fix Risk
- If cleanup is duplicated in both handler paths and disconnect path, double-remove bugs may appear.
- Better to define one canonical “connection teardown” routine.

### Verification
- Repeated connect/disconnect should return `UserManager`, `SessionManager`, and connected-client counts to baseline.

---

## 2) Server shutdown 시 active client를 명시적으로 close/drain 하지 않는 문제

### Evidence
- Chat server stop order: `exec/chat/chat-server/chat-server/Server.cpp:49-61`
- `ServerLifeCycle::Stop()` only shuts down acceptor/IOCP and clears `_clientPool`: `network/src/ServerLifecycle.cpp:126-139`
- It does not iterate active pooled clients and close them before teardown: `network/src/ServerLifecycle.cpp:126-139`

### Root Cause
`ServerLifeCycle` currently destroys infrastructure before explicitly draining owned connections.

The current stop model is roughly:

`acceptor shutdown -> iocp shutdown -> clear pool`

But a correct resource model is usually:

`stop new accepts -> stop/close active clients -> drain or cancel completions -> stop worker infrastructure -> destroy containers`

The current version skips the explicit “close active clients” stage.

### Why This Happens
`_clientPool` is treated partly like ownership storage and partly like a slot registry.

That is dangerous:
- clearing the vector only releases **one ownership source**
- it does **not** guarantee destruction if other `shared_ptr<Client>` owners still exist

So `_clientPool.clear()` looks like cleanup, but in reality it is only partial deregistration.

### Fix Direction
Redesign `ServerLifeCycle::Stop()` to do staged shutdown:

1. mark lifecycle as stopping
2. stop posting new accepts
3. iterate all active clients in `_clientPool`
4. close each connected socket explicitly
5. wait for or safely cancel outstanding completions
6. shutdown IOCP
7. clear pool

### Affected Files
- `network/src/ServerLifecycle.cpp`
- possibly `network/inc/server/ServerLifecycle.h` for explicit stopping state
- `exec/chat/chat-server/chat-server/Server.cpp` for stop ordering

### Fix Risk
- Closing clients too late can leave completions targeting half-destroyed objects.
- Closing clients too early without a stopping flag can trigger duplicate disconnect callbacks.

### Verification
- Stop the server while clients are connected.
- Verify all sockets are closed before lifecycle destruction completes.

---

## 3) `AcceptEx` pending socket / overlapped shutdown semantics 문제

### Evidence
- `PostAccept()` creates a fresh accept socket and stores it in `AcceptOverlapped::clientSocket`: `network/src/acceptor/IocpAcceptor.cpp:92-150`
- `AcceptOverlapped` contains the accepted socket handle field: `network/inc/client/windows/OverlappedExt.h:51-57`
- Held accepts are stored in `DeferredItemHolder<AcceptOverlapped>`: `network/inc/acceptor/IocpAcceptor.h:148-149`, `lib/inc/scope/DeferredItemHolder.hpp:11-27`
- The pool-return path only returns memory to the pool; it does not close `clientSocket`: `lib/inc/memory/HybridObjectPool.hpp:53-95`, `lib/inc/scope/DeferContext.hpp:21-31`
- `Shutdown()` only cancels I/O and nulls function pointers: `network/src/acceptor/IocpAcceptor.cpp:241-254`

### Root Cause
The overlapped object owns **two different lifetimes**, but only one is actually modeled:

1. memory lifetime of `AcceptOverlapped`
2. handle lifetime of `clientSocket`

Current code properly models #1 with pool-return semantics.
Current code does **not** properly model #2 during shutdown.

That means the code remembers “who returns the overlapped memory,” but not “who closes the socket if completion never reaches the normal success path.”

### Why This Happens
The happy path is correct:

`AcceptEx completes -> OnAcceptComplete() -> transfer socket to server -> set overlapped clientSocket invalid`

But shutdown is a different path:

`AcceptEx pending -> CancelIoEx() -> maybe no normal accept-complete transfer`

When that happens, the socket stored inside the overlapped may never be closed.

### Additional Ordering Problem
- `ServerLifeCycle::Stop()` destroys acceptor before IOCP shutdown: `network/src/ServerLifecycle.cpp:126-134`

So canceled accept completions may still be in flight while the acceptor’s holder/state is being destroyed. That creates a stale-overlapped race window.

### Fix Direction
Make accept shutdown own both memory and socket cleanup.

Recommended approach:
1. In `IocpAcceptor::Shutdown()`, walk every held `AcceptOverlapped`.
2. Cancel I/O.
3. Explicitly close `clientSocket` if still valid.
4. Mark `clientSocket = InvalidSocket`.
5. Keep acceptor state alive until IOCP workers are drained, or change stop order so IOCP is shut down/drained before acceptor destruction finishes.

### Affected Files
- `network/src/acceptor/IocpAcceptor.cpp`
- `network/src/ServerLifecycle.cpp`
- possibly `network/inc/acceptor/IocpAcceptor.h`

### Fix Risk
- Double-close is possible if both completion path and shutdown path try to own the same socket.
- The fix must establish one rule: whoever successfully transfers the socket clears ownership.

### Verification
- Start server, leave many accepts pending, stop server, check handle count / no leaked sockets / no stale completion access.

---

## 4) Echo server가 같은 recv overlapped를 이중 게시하는 문제

### Evidence
- `ServerLifeCycle::HandleRecv()` reposts `PostRecv()` after app callback: `network/src/ServerLifecycle.cpp:240-248`
- Echo app also reposts `PostRecv()`: `exec/echo/echo-server/Server.cpp:57-60`
- `Client` has only one recv overlapped object: `network/inc/client/windows/Client.h:91-95`

### Root Cause
The ownership of “who is allowed to repost recv” is ambiguous.

Both layers think they own the recv pipeline:

- network lifecycle thinks: “I own the next recv”
- echo app thinks: “I must repost recv after handling”

That causes two `WSARecv()` submissions to reuse the same `recvOverlapped`.

### Why This Happens
There is no clear contract for recv responsibility.

In IOCP code, overlapped I/O state is not just data—it is an in-flight ownership token.
If one recv overlapped object is already in flight, reposting another recv with the same overlapped state is a correctness violation.

### Fix Direction
Choose one owner for recv reposting.

Recommended rule:
- `ServerLifeCycle` owns recv reposting
- app handlers never call `PostRecv()` directly unless the lifecycle contract explicitly says so

So the fix is simple:
- remove the extra `PostRecv()` from `exec/echo/echo-server/Server.cpp`

### Affected Files
- `exec/echo/echo-server/Server.cpp`
- optionally comments/docs in `ServerLifecycle` to make the contract explicit

### Fix Risk
- Very low. Mostly contract clarification.

### Verification
- Ensure only one outstanding recv exists per client under logging/instrumentation.

---

## 5) `Client` callbacks가 `ServerLifeCycle`의 raw `this`를 잡는 문제

### Evidence
- `BuildClientCallbacks()` captures `[this]`: `network/src/ServerLifecycle.cpp:19-55`
- callbacks are copied into pooled `Client`s: `network/src/ServerLifecycle.cpp:66-70`, `111-114`
- leaked clients can outlive lifecycle teardown due to findings 1 and 2

### Root Cause
The callback edge from `Client` back to `ServerLifeCycle` is non-owning, but it behaves like it assumes the lifecycle always outlives the client.

That assumption is false in this codebase because:
- `Client` is shared-owned
- application code can retain it indirectly
- `ServerLifeCycle` is unique-owned and can be destroyed earlier

So the callback edge is:

`Client -> raw captured this -> ServerLifeCycle`

which is a classic dangling-lifetime hazard.

### Why This Happens
The callback design is optimized for convenience, not for explicit lifetime safety.

This would be safe only if:
1. `ServerLifeCycle` strictly outlived every `Client`
2. and no `Client` could escape lifecycle ownership

Neither is currently guaranteed.

### Fix Direction
Any one of these is acceptable, but the codebase should pick one consistently:

1. **Best structural fix**: eliminate leaked `Client`s first (findings 1 and 2), then explicitly clear client callbacks before lifecycle destruction.
2. Replace raw `this` callback capture with a safer indirection/state object whose lifetime is controlled.
3. Add a stopping/dead flag checked by callbacks before touching lifecycle state.

Practical first fix:
- before destroying `ServerLifeCycle`, clear client callbacks on all pooled clients
- after that, continue with lifecycle teardown

### Affected Files
- `network/src/ServerLifecycle.cpp`
- `network/inc/client/windows/Client.h` if callback reset API is added

### Fix Risk
- If callbacks are cleared too early, metrics/accounting may stop updating before all completions settle.
- So callback clearing must happen in a deliberate shutdown stage.

### Verification
- Shutdown with leaked-client scenarios should not fire callbacks into destroyed lifecycle memory.

---

## 6) per-client send in-flight ownership guard 부재

### Evidence
- `Client` has one `sendOverlapped` and one fixed send buffer: `network/inc/client/windows/Client.h:94-95`
- `PostSend()` overwrites that same buffer on every call with no pending-send guard: `network/src/client/windows/Client.cpp:99-125`
- Higher layers may call send repeatedly through `Session::Send()` and room broadcast loops: `exec/chat/chat-server/chat-server/Session.cpp:38-49`, `exec/chat/chat-server/chat-server/room/Room.cpp:30-34`, `61-67`, `77-83`

### Root Cause
The networking layer assumes either:
- only one send per client can happen at a time
- or higher layers serialize sends

But it does not enforce either rule.

That means buffer ownership is implicit rather than explicit.

### Why This Happens
In overlapped send I/O, the send buffer must remain stable until completion.

Current `PostSend()` does:
1. copy data into `sendOverlapped.buf`
2. call `WSASend()`
3. return immediately

If another `PostSend()` runs before completion, it may overwrite the same in-flight buffer.

### Fix Direction
Introduce an explicit per-client send ownership model.

Options:
1. **single-flight guard + queue** (recommended)
   - if a send is in flight, enqueue payload
   - on send completion, pop next queued payload and post it
2. mutex + “only one outstanding send” assertion
3. allocate/send separate buffers per request

For this codebase, option 1 is the most maintainable.

### Affected Files
- `network/inc/client/windows/Client.h`
- `network/src/client/windows/Client.cpp`
- possibly `network/src/ServerLifecycle.cpp` for completion-driven send progression

### Fix Risk
- Introducing a send queue changes ordering and backpressure behavior.
- Must define what happens when queue grows too large.

### Verification
- Multi-send stress test to the same client must preserve order and avoid buffer corruption.

---

## 7) invalid packet / frame error 경로가 app cleanup을 우회하는 문제

### Evidence
- `PacketDispatcher::Receive()` closes client directly on frame overflow / payload-too-large / invalid packet: `network/src/server/PacketDispatcher.cpp:23-29`, `46-55`, `80-86`
- `Client::Close()` only closes socket and emits local disconnect event; it does not notify app-layer `OnDisconnect`: `network/src/client/windows/Client.cpp:127-150`
- App-layer disconnect cleanup lives in `GameLoop::Disconnect()`: `exec/chat/chat-server/chat-server/GameLoop.cpp:128-137`

### Root Cause
There are two different disconnect paths:

1. **network-driven/app-aware path**
   - recv completion sees disconnect
   - `ServerLifeCycle::HandleRecv()`
   - `_handler->OnDisconnect(client)`
   - app cleanup happens

2. **dispatcher-forced direct close path**
   - `PacketDispatcher` detects bad input
   - `client->Close(true)` directly
   - app cleanup does not happen

So abnormal disconnect is not routed through the same teardown pipeline as normal disconnect.

### Why This Happens
The code currently treats “close the socket” and “tear down the session” as if they were the same operation.

They are not the same:
- socket close = transport action
- disconnect cleanup = ownership cleanup across app/network layers

### Fix Direction
Unify forced close with the canonical disconnect path.

Recommended approach:
1. `PacketDispatcher` should not directly own session teardown.
2. It should signal lifecycle/app layer that the client must be disconnected.
3. The lifecycle should perform:
   - notify app (`OnDisconnect`)
   - close socket
   - mark client reusable only after cleanup rules are satisfied

Possible implementation choices:
- return an error to lifecycle and let lifecycle disconnect
- inject a disconnect callback into `PacketDispatcher`
- add a small “CloseClientAndNotify” helper at the lifecycle boundary

### Affected Files
- `network/src/server/PacketDispatcher.cpp`
- `network/src/ServerLifecycle.cpp`
- maybe `network/inc/server/PacketDispatcher.hpp`

### Fix Risk
- If both dispatcher and lifecycle try to close the same client, duplicate disconnect handling can occur.
- The fix should ensure one disconnect authority.

### Verification
- Send malformed/oversized packet.
- Verify room/session/user counts return to baseline exactly as they do for graceful disconnect.

---

## Implementation Phases

### Phase 1 — Disconnect ownership correctness
- Fix findings 1 and 7 together.
- Goal: every disconnect path, graceful or forced, reaches the same app cleanup sequence.

### Phase 2 — Shutdown correctness
- Fix findings 2, 3, and 5 together.
- Goal: stop order closes active clients, drains/cancels completions safely, and avoids dangling callback edges.

### Phase 3 — In-flight I/O ownership correctness
- Fix findings 4 and 6.
- Goal: exactly one recv repost owner and explicit send serialization.

## Verification Plan
1. Repeated connect/disconnect soak test
2. Malformed packet disconnect test
3. Stop server with active clients attached
4. Stop server while accepts are pending
5. Same-client rapid multi-send stress test
6. Echo recv-path test with outstanding-recv instrumentation

## ADR
- **Decision**: fix disconnect-path ownership first, then shutdown, then in-flight I/O serialization.
- **Drivers**: ownership correctness, handle release, shutdown safety, minimal regression risk.
- **Alternatives considered**:
  - fix acceptor shutdown first
  - fix send queue first
  - fix only chat-side leak first
- **Why chosen**: findings 1 and 7 are runtime-path bugs that poison nearly every later lifecycle step; fixing them first reduces the chance of misdiagnosing shutdown behavior.
- **Consequences**: some fixes require API-boundary cleanup refactoring, not just local line edits.
- **Follow-ups**: after implementation, add deterministic teardown tests or at least repeatable manual harnesses for disconnect/shutdown scenarios.

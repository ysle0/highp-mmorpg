# shared_ptr Passing Convention Guide

This document traces `shared_ptr<Client>` through the entire I/O pipeline, counts every atomic ref-count operation, and defines a mechanical rule for when to pass by value vs. by `const&`.

---

## The Problem

Every time a `shared_ptr` is **copied**, two atomic operations occur: an atomic increment on construction and an atomic decrement on destruction. On x86 these are `lock xadd` instructions that force cache-line synchronization across cores. In a hot path processing thousands of packets/sec from multiple IOCP worker threads, these add up.

---

## Current Flow: One Recv Event (Echo Server)

### Call Chain

```
ServerLifecycle.cpp:154  HandleRecv()
  │
  ├─ client->shared_from_this()              // atomic inc: ref 1→2
  │
  ├─ _handler->OnRecv(clientPtr, data)       // COPY into by-value param: atomic inc 2→3
  │   │
  │   │  [Server::OnRecv(shared_ptr<Client> client, ...)]
  │   │
  │   ├─ client->PostSend(...)               // dereference only, no ref change
  │   ├─ _lifecycle->CloseClient(client, ..) // COPY into by-value param: atomic inc 3→4
  │   │                                      // CloseClient returns, param destroyed: atomic dec 4→3
  │   │
  │   └─ return                              // OnRecv param destroyed: atomic dec 3→2
  │
  └─ return                                  // clientPtr destroyed: atomic dec 2→1
```

**Total: 6 atomic operations** (3 inc + 3 dec) per recv event.

### Dispatcher Path (Chat Server)

```
ServerLifecycle.cpp:159  shared_from_this()           // atomic inc: ref 1→2
ServerLifecycle.cpp:177  OnRecv(clientPtr, data)       // COPY: atomic inc 2→3
  │
  PacketDispatcher.cpp:13  Receive(shared_ptr client)  // already copied above
  │
  ├─ PushCommand(client, ...)                          // COPY: atomic inc 3→4
  │   ├─ PacketCommand { .client = client }            // COPY: atomic inc 4→5
  │   ├─ _commandQueue.Push(std::move(cmd))            // move: 0
  │   └─ return                                        // PushCommand param destroyed: dec 5→4
  │
  └─ return                                            // Receive param destroyed: dec 4→3
                                                       // OnRecv param destroyed: dec 3→2
                                                       // clientPtr destroyed: dec 2→1

--- later, in logic thread ---

PacketDispatcher.cpp:73  Tick()
  for (auto& cmd : _tickBatch)                         // ref, no copy
  │
  ├─ DispatchPacket(cmd.client, packet)                // COPY: atomic inc 1→2
  │   └─ it->second(std::move(client), packet)         // move: 0
  │       └─ handler->Handle(shared_ptr client, ...)   // already moved, 0
  │   return                                           // DispatchPacket param destroyed: dec 2→1
  │
  └─ ~cmd                                             // PacketCommand destroyed: dec 1→0
```

**Total: 10 atomic operations** (5 inc + 5 dec) per packet through the dispatcher.

---

## The Rule

```
Does this function STORE the shared_ptr beyond its return?
│
├─ YES (cross-thread transfer, collection insertion)
│   └─ Pass by VALUE
│       Caller can std::move if it no longer needs it.
│
├─ NO, but it FORWARDS to another by-value sink
│   └─ Pass by VALUE (enables the std::move at the forward site)
│
└─ NO (just dereferences and returns)
    └─ Pass by CONST REF (zero atomic ops)
```

### Why `const&` Is Safe Here

When function `A` calls function `B(const shared_ptr<T>& p)`, the safety requirement is:

> **A's shared_ptr must outlive the call to B.**

This is trivially true when B is a synchronous call and A holds the shared_ptr on its stack. The `HandleRecv()` function holds `clientPtr` as a local variable — it outlives every callback it invokes.

### When You MUST Copy

1. **Cross-thread ownership transfer**: `PacketCommand.client` stores the ptr in a struct that the logic thread will consume later. The worker thread's `clientPtr` will be destroyed after `Push()` returns, so the command needs its own copy.

2. **Stored in a collection with independent lifetime**: e.g., a `std::unordered_map<int, shared_ptr<Client>>` of active connections.

3. **Forwarded into a std::move sink**: `DispatchPacket` takes by value because it `std::move`s into the handler lambda. Taking by `const&` and then copying at the move site would be equivalent, but by-value + move is more idiomatic.

---

## Call Site Audit

### `ISessionEventReceiver.h` — Interface Callbacks

| Method | Current | Should Be | Reason |
|--------|---------|-----------|--------|
| `OnAccept(shared_ptr<Client>)` | by value | `const&` | Caller holds ptr on stack, callback just logs/uses |
| `OnRecv(shared_ptr<Client>, ...)` | by value | `const&` | Same — caller outlives call |
| `OnSend(shared_ptr<Client>, ...)` | by value | `const&` | Same |
| `OnDisconnect(shared_ptr<Client>)` | by value | `const&` | Same |

### `PacketDispatcher.hpp` — Dispatcher Methods

| Method | Current | Should Be | Reason |
|--------|---------|-----------|--------|
| `Receive(shared_ptr<Client>, ...)` | by value | `const&` | Copies into PushCommand internally, but parameter itself isn't stored |
| `PushCommand(shared_ptr<Client>, ...)` | by value | `const&` | Copies into PacketCommand — that's the *intentional* copy site |
| `DispatchPacket(shared_ptr<Client>, ...)` | by value | **by value** (keep) | `std::move`s into handler lambda at line 118 |
| `DispatchFn` lambda param | by value | **by value** (keep) | Receives the moved ptr from DispatchPacket |

### `IPacketHandler.hpp` — Handler Interface

| Method | Current | Should Be | Reason |
|--------|---------|-----------|--------|
| `Handle(shared_ptr<Client>, ...)` | by value | `const&` | Handler just uses the client, doesn't store it |

### `RegisterHandler` Lambda in `PacketDispatcher.hpp`

```cpp
// Current (line 45-53):
_handlers[key] = [handler](shared_ptr<Client> client, const protocol::Packet* packet) {
    handler->Handle(client, payload);       // copies again into Handle's by-value param
};

// Fixed:
_handlers[key] = [handler](shared_ptr<Client> client, const protocol::Packet* packet) {
    handler->Handle(client, payload);       // Handle now takes const&, no copy
};
```

The lambda parameter stays by-value because `DispatchPacket` moves into it. But `Handle()` changes to `const&`, so the copy inside the lambda body is eliminated.

### `ServerLifeCycle` Methods

| Method | Current | Should Be | Reason |
|--------|---------|-----------|--------|
| `CloseClient(shared_ptr<Client>, bool)` | by value | `const&` | Just calls Close() and decrements counter |

---

## After the Fix: Atomic Ops Per Packet

### Echo Server Path

```
HandleRecv:
  shared_from_this()          → +1 inc  (unavoidable)
  OnRecv(const& clientPtr)    →  0      (was: +1 inc, +1 dec)
    PostSend / PostRecv       →  0
    CloseClient(const&)       →  0      (was: +1 inc, +1 dec)
  ~clientPtr                  → +1 dec

Total: 2 atomic ops (down from 6)    ← 3x reduction
```

### Dispatcher Path

```
HandleRecv:
  shared_from_this()            → +1 inc
  OnRecv(const&)                →  0       (was: +1 inc, +1 dec)

Receive(const&):
  PushCommand(const&)           →  0       (was: +1 inc, +1 dec)
    PacketCommand{.client=ref}  → +1 inc   (intentional cross-thread copy)

  ~clientPtr in HandleRecv      → +1 dec

Tick:
  DispatchPacket(move from cmd) →  0       (moved from PacketCommand)
    lambda(already moved)       →  0
      Handle(const&)            →  0       (was: +1 inc, +1 dec)
    ~DispatchPacket param       → +1 dec

Total: 4 atomic ops (down from 10)    ← 2.5x reduction
```

---

## Summary of Changes

| File | Change |
|------|--------|
| `network/inc/server/ISessionEventReceiver.h` | All 4 methods: `shared_ptr<Client>` → `const shared_ptr<Client>&` |
| `network/inc/server/PacketDispatcher.hpp` | `Receive()`, `PushCommand()`: → `const&`. `DispatchPacket()`: keep by-value. `Handle()` call in lambda: no change needed (Handle takes `const&` now). `DispatchFn` typedef: keep by-value (receives move) |
| `network/inc/server/IPacketHandler.hpp` | `Handle()`: → `const shared_ptr<Client>&` |
| `network/inc/server/ServerLifecycle.h` | `CloseClient()`: → `const shared_ptr<Client>&` |
| `exec/echo/echo-server/Server.h` | All 4 overrides: match new `const&` interface |
| `exec/chat/chat-server/chat-server/Server.h` | All overrides: match new `const&` interface |

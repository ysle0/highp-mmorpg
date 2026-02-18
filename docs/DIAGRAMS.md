# Visual Architecture Diagrams

Comprehensive visual reference for system architecture, data flow, and component relationships.

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Component Architecture](#component-architecture)
3. [Data Flow Diagrams](#data-flow-diagrams)
4. [Threading Model](#threading-model)
5. [IOCP Event Loop](#iocp-event-loop)
6. [Protocol Stack](#protocol-stack)
7. [Configuration Flow](#configuration-flow)

---

## System Overview

### High-Level Three-Tier Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Application Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │ EchoServer   │  │  ChatServer  │  │ GameServer   │           │
│  │              │  │  (planned)   │  │  (planned)   │           │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘           │
│         │                 │                  │                   │
│         │          implements IServerHandler │                   │
│         └─────────────────┼──────────────────┘                   │
└───────────────────────────┼──────────────────────────────────────┘
                            │
┌───────────────────────────┼──────────────────────────────────────┐
│                      Network Layer                               │
│         ┌─────────────────▼─────────────────┐                    │
│         │       ServerCore                  │                    │
│         │   (Lifecycle Management)          │                    │
│         └────┬──────────────────────┬───────┘                    │
│              │                      │                            │
│    ┌─────────▼────────┐   ┌────────▼──────────┐                 │
│    │ IocpIoMultiplexer│   │  IocpAcceptor     │                 │
│    │ (IOCP + Workers) │   │  (AcceptEx)       │                 │
│    └─────────┬────────┘   └────────┬──────────┘                 │
│              │                      │                            │
│              │         ┌────────────▼──────────┐                 │
│              │         │      Client           │                 │
│              │         │  (Connection State)   │                 │
│              │         └───────────────────────┘                 │
│              │                                                   │
│    ┌─────────▼──────────────────────────────────┐               │
│    │         IOCP Kernel Object                 │               │
│    │  (GetQueuedCompletionStatus)               │               │
│    └────────────────────────────────────────────┘               │
└─────────────────────────────────────────────────────────────────┘
                            │
┌───────────────────────────┼──────────────────────────────────────┐
│                    Utilities Layer                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐         │
│  │ Logger   │  │ Result<> │  │ObjectPool│  │  Errors  │         │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘         │
└─────────────────────────────────────────────────────────────────┘
```

---

## Component Architecture

### Component Ownership and Lifecycle

```
EchoServer (Application)
  │
  ├─ owns unique_ptr<ServerCore>
  │    │
  │    ├─ owns unique_ptr<IocpIoMultiplexer>
  │    │    └─ owns vector<jthread> (worker threads)
  │    │
  │    ├─ owns unique_ptr<IocpAcceptor>
  │    │    └─ owns ObjectPool<AcceptOverlapped>
  │    │
  │    └─ owns vector<shared_ptr<Client>>
  │         └─ shared between application and worker threads
  │
  └─ implements IServerHandler
       ├─ OnAccept(client)
       ├─ OnRecv(client, data)
       ├─ OnSend(client, bytes)
       └─ OnDisconnect(client)
```

### Component Interaction Sequence

```
New Client Connection Flow:

┌────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   Client   │────>│ IocpAcceptor │────>│   ServerCore │────>│ IServerHandler│
│  (remote)  │     │  (AcceptEx)  │     │ (orchestrate)│     │ (EchoServer) │
└────────────┘     └──────────────┘     └──────────────┘     └──────────────┘
      │                   │                     │                     │
      │ TCP SYN           │                     │                     │
      ├──────────────────>│                     │                     │
      │                   │ AcceptEx completes  │                     │
      │                   ├────────────────────>│                     │
      │                   │                     │ FindAvailableClient │
      │                   │                     │ AssociateSocket     │
      │                   │                     │ PostRecv()          │
      │                   │                     ├────────────────────>│
      │                   │                     │    OnAccept(client) │
      │<──────────────────┴─────────────────────┴─────────────────────┤
      │ Connection established                                        │
```

---

## Data Flow Diagrams

### Echo Message Complete Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. Client Send                                                  │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. TCP Network                                                  │
│    - Data arrives at server NIC                                 │
│    - Kernel buffers data                                        │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. IOCP Kernel Object                                           │
│    - Recv operation completes                                   │
│    - Completion queued to IOCP                                  │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4. Worker Thread                                                │
│    - GetQueuedCompletionStatus() returns                        │
│    - Extract: completionKey, overlapped, bytesTransferred       │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 5. IocpIoMultiplexer::WorkerLoop()                              │
│    - Build CompletionEvent                                      │
│    - Invoke registered CompletionHandler                        │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. ServerCore::OnCompletion()                                   │
│    - Route by ioType (Accept, Recv, Send)                       │
│    - Extract Client* from completionKey                         │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 7. ServerCore::HandleRecv()                                     │
│    - Extract data from RecvOverlapped.buf                       │
│    - Call IServerHandler::OnRecv(client, data)                  │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 8. EchoServer::OnRecv()                                         │
│    - Business logic: Echo data back                             │
│    - client->PostSend(data)                                     │
│    - client->PostRecv() (continue receiving)                    │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 9. Client::PostSend()                                           │
│    - Copy data to sendOverlapped.buf                            │
│    - WSASend(socket, sendOverlapped)                            │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 10. TCP Network                                                 │
│     - Data sent to client                                       │
│     - Send completion queued to IOCP                            │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 11. Send Completion (similar to recv)                           │
│     - Worker thread handles send completion                     │
│     - Calls IServerHandler::OnSend(client, bytes)               │
└─────────────────────────────────────────────────────────────────┘
```

---

## Threading Model

### Thread Pool Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         IOCP Kernel Object                      │
│                     (Completion Queue)                          │
└────────────────────┬────────────────────────────────────────────┘
                     │
         ┌───────────┼───────────┬───────────┬───────────┐
         │           │           │           │           │
    ┌────▼────┐ ┌────▼────┐ ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
    │Worker 1 │ │Worker 2 │ │Worker 3 │ │Worker 4 │ │Worker N │
    └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘
         │           │           │           │           │
         │ GQCS()    │ GQCS()    │ GQCS()    │ GQCS()    │ GQCS()
         │ (blocked) │ (blocked) │ (blocked) │ (blocked) │ (blocked)
         │           │           │           │           │
         └───────────┴───────────┴───────────┴───────────┘
                     │
                     ▼
         ┌────────────────────────┐
         │  CompletionHandler     │
         │  ServerCore::OnCompletion()
         └────────────────────────┘
                     │
         ┌───────────┼───────────┐
         │           │           │
    ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
    │ Accept  │ │  Recv   │ │  Send   │
    │ Handler │ │ Handler │ │ Handler │
    └─────────┘ └─────────┘ └─────────┘
```

### Worker Thread Lifecycle

```
Worker Thread State Machine:

    ┌──────────────┐
    │   Created    │
    └──────┬───────┘
           │
           ▼
    ┌──────────────┐
    │  Blocked on  │<─────────┐
    │    GQCS()    │          │
    └──────┬───────┘          │
           │                  │
           │ (I/O completes)  │
           ▼                  │
    ┌──────────────┐          │
    │   Process    │          │
    │  Completion  │          │
    └──────┬───────┘          │
           │                  │
           ├──────────────────┘
           │ (continue loop)
           │
           │ (shutdown signal)
           ▼
    ┌──────────────┐
    │  Terminated  │
    └──────────────┘
```

---

## IOCP Event Loop

### IOCP Worker Thread Event Loop

```cpp
// Simplified worker loop visualization
while (!shutdownRequested) {
    ┌────────────────────────────────────────┐
    │  GetQueuedCompletionStatus()           │
    │  - Block waiting for I/O completion    │
    │  - Return: bytes, key, overlapped      │
    └─────────────┬──────────────────────────┘
                  │
                  ▼
    ┌────────────────────────────────────────┐
    │  Check for shutdown signal             │
    │  - key == nullptr && overlapped == null│
    └─────────────┬──────────────────────────┘
                  │ No shutdown
                  ▼
    ┌────────────────────────────────────────┐
    │  Build CompletionEvent                 │
    │  - Extract ioType from overlapped      │
    │  - Extract Client* from completionKey  │
    └─────────────┬──────────────────────────┘
                  │
                  ▼
    ┌────────────────────────────────────────┐
    │  Invoke CompletionHandler              │
    │  - ServerCore::OnCompletion(event)     │
    └─────────────┬──────────────────────────┘
                  │
                  └────────────────────────────> (loop back)
}
```

---

## Protocol Stack

### Network Layer to Application Layer

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Layer                            │
│  ┌──────────────────────────────────────────────────────┐       │
│  │           Business Logic (EchoServer)                │       │
│  │  - OnAccept: Log connection                          │       │
│  │  - OnRecv: Echo data back                            │       │
│  │  - OnSend: Log throughput                            │       │
│  └──────────────────────────────────────────────────────┘       │
└───────────────────────────┬─────────────────────────────────────┘
                            │ IServerHandler interface
┌───────────────────────────┼─────────────────────────────────────┐
│                    Network Layer                                │
│  ┌────────────────────────▼──────────────────────────────┐      │
│  │           ServerCore (Orchestrator)                   │      │
│  │  - Route completions to handlers                      │      │
│  │  - Manage client lifecycle                            │      │
│  └────────────┬────────────────────┬─────────────────────┘      │
│               │                    │                            │
│  ┌────────────▼──────────┐  ┌──────▼──────────────┐            │
│  │  IocpIoMultiplexer    │  │   IocpAcceptor      │            │
│  │  - Worker threads     │  │   - AcceptEx loop   │            │
│  │  - IOCP event loop    │  │   - Socket pool     │            │
│  └────────────┬──────────┘  └──────┬──────────────┘            │
│               │                    │                            │
│               └─────────┬──────────┘                            │
│                         │                                       │
│  ┌──────────────────────▼──────────────────────┐                │
│  │            IOCP Kernel Object               │                │
│  │  - Completion queue                         │                │
│  │  - Thread wake-up management                │                │
│  └──────────────────────┬──────────────────────┘                │
└─────────────────────────┼───────────────────────────────────────┘
                          │ Windows Kernel
┌─────────────────────────┼───────────────────────────────────────┐
│                 Winsock / TCP Stack                             │
│  - Socket operations (WSARecv, WSASend, AcceptEx)               │
│  - TCP connection management                                    │
│  - Network buffer management                                    │
└─────────────────────────────────────────────────────────────────┘
```

---

## Configuration Flow

### Configuration System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                  Configuration Sources                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌────────────────────┐              ┌────────────────────┐     │
│  │ config.compile.toml│              │config.runtime.toml │     │
│  │  (Buffer sizes)    │              │ (Server settings)  │     │
│  └─────────┬──────────┘              └─────────┬──────────┘     │
│            │                                   │                │
│            │ parse_compile_cfg.ps1             │                │
│            │                                   │                │
│            ▼                                   │                │
│  ┌────────────────────┐                       │                │
│  │    Const.h         │                       │                │
│  │  (constexpr)       │              parse_network_cfg.ps1     │
│  └────────────────────┘                       │                │
│            │                                   ▼                │
│            │                         ┌────────────────────┐     │
│            │                         │   NetworkCfg.h     │     │
│            │                         │  (runtime struct)  │     │
│            │                         └─────────┬──────────┘     │
│            │                                   │                │
│            │                                   │                │
│            │    ┌──────────────────┐           │                │
│            └────►  C++ Compiler    ◄───────────┘                │
│                 └────────┬─────────┘                            │
│                          │                                      │
│                          ▼                                      │
│                 ┌────────────────┐                              │
│                 │  echo-server   │                              │
│                 │  (executable)  │                              │
│                 └────────┬───────┘                              │
│                          │                                      │
│                          │ reads at runtime                     │
│                          ▼                                      │
│                 ┌────────────────┐                              │
│                 │config.runtime  │                              │
│                 │    .toml       │◄─────── Environment          │
│                 └────────────────┘         Variables            │
│                                            (overrides)           │
└─────────────────────────────────────────────────────────────────┘
```

---

## Memory Layout

### Per-Client Memory Allocation

```
Client Object (std::shared_ptr<Client>):

┌─────────────────────────────────────────────────────────────────┐
│                         Client Instance                         │
├─────────────────────────────────────────────────────────────────┤
│  SocketHandle socket                              [8 bytes]     │
├─────────────────────────────────────────────────────────────────┤
│  RecvOverlapped recvOverlapped                    [~4120 bytes] │
│    ├─ WSAOVERLAPPED overlapped                    [32 bytes]    │
│    ├─ EIoType ioType                              [4 bytes]     │
│    ├─ SocketHandle clientSocket                   [8 bytes]     │
│    ├─ WSABUF bufDesc                              [16 bytes]    │
│    └─ char buf[4096]                              [4096 bytes]  │
├─────────────────────────────────────────────────────────────────┤
│  SendOverlapped sendOverlapped                    [~1080 bytes] │
│    ├─ WSAOVERLAPPED overlapped                    [32 bytes]    │
│    ├─ EIoType ioType                              [4 bytes]     │
│    ├─ SocketHandle clientSocket                   [8 bytes]     │
│    ├─ WSABUF bufDesc                              [16 bytes]    │
│    └─ char buf[1024]                              [1024 bytes]  │
├─────────────────────────────────────────────────────────────────┤
│  Total per Client:                                ~5208 bytes   │
│                                                   (~5 KB)        │
└─────────────────────────────────────────────────────────────────┘

Total Memory for 1000 clients: ~5 MB
Total Memory for 10000 clients: ~50 MB
```

---

## See Also

- **[Architecture Overview](ARCHITECTURE.md)** - Detailed architecture documentation
- **[Network Layer Guide](NETWORK_LAYER.md)** - Implementation details
- **[IOCP Echo Server Architecture](IOCP_EchoServer_Architecture.md)** - Echo server specifics
- **[Echo Server Sequence Diagrams](EchoServer_Sequence_diagram.md)** - Sequence diagrams

---

**Last Updated:** 2026-02-05

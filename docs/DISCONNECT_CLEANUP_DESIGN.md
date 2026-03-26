# Disconnect 시 User Cleanup 설계

## 배경

`OnDisconnect` 발생 시 해당 TCP Socket에 연결된 User를 찾아 정리(destroy)해야 한다.
SessionManager를 통해 Socket → Session → User 매핑을 관리하고, disconnect 이벤트를 Logic Thread에서 처리한다.

## 핵심 원칙

- **모든 상태 변경은 Logic Thread에서만** 수행 (기존 PacketDispatcher 패턴과 동일)
- Worker Thread에서는 disconnect 이벤트를 MPSC 큐에 enqueue만 수행
- Lock 없이 thread-safe한 구조 유지

## 데이터 구조

```
SessionManager
├── unordered_map<SocketHandle, Session*>  _sessionsBySocket
│
Session
├── shared_ptr<Client>  client
├── User*               user       // nullable (인증 전에는 없을 수 있음)
├── uint64_t            sessionId
│
User
├── uint64_t   userId
├── string     username
├── UserStatus status
├── Room*      currentRoom   // nullable
```

## Disconnect 처리 흐름

```
[Worker Thread]
  IOCP recv → bytesTransferred == 0 (graceful disconnect)
    → ServerLifecycle::HandleRecv()
      → OnDisconnect(client)  ← ISessionEventReceiver 콜백
        → DisconnectCommand { client->socket } 를 MPSC 큐에 enqueue

[MPSC Queue]
  DisconnectCommand 전달

[Logic Thread]
  Tick()에서 drain
    → SessionManager.FindBySocket(socketHandle)
    → Session 발견 시:
      1. User가 Room에 있으면 Room에서 제거
      2. 같은 Room의 다른 User들에게 UserLeftBroadcast 전송
      3. User 객체 해제
      4. Session 객체 해제
      5. SessionManager에서 매핑 제거
```

## 구현 포인트

### 1. PacketDispatcher 확장

기존 PacketDispatcher의 MPSC 큐를 활용하되, FlatBuffers 패킷이 아닌 내부 이벤트(Disconnect)도 전달할 수 있도록 Command 타입을 확장한다.

```cpp
enum class CommandType { Packet, Disconnect };

struct Command {
    CommandType type;
    std::shared_ptr<Client> client;
    // Packet인 경우에만 사용
    protocol::Payload payloadType;
    std::vector<uint8_t> payload;
};
```

### 2. Session 생성 시점

`OnAccept` → MPSC 큐 → Logic Thread에서 Session 생성.
Socket 연결 즉시 Session을 만들고, 이후 인증 패킷 수신 시 User를 Session에 바인딩한다.

### 3. 순서 보장

동일 Client에 대해 패킷 처리와 disconnect 처리가 MPSC 큐를 통해 순서대로 Logic Thread에서 실행되므로, 마지막 패킷 처리 후 disconnect가 보장된다.

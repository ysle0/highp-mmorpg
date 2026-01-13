# IOCP Echo Server Architecture

## 개요
Windows IOCP(I/O Completion Port) 기반의 고성능 비동기 Echo 서버 구현

---

## 0. 핵심 포인트 요약

| 항목 | 설명 |
|------|------|
| **IOCP 연결** | Listen 소켓 + 각 Client 소켓 모두 IOCP에 연결 필요 |
| **AcceptEx 간접 호출** | WSAIoctl로 함수 포인터 획득 후 호출 |
| **OverlappedExt 캐스팅** | WSAOVERLAPPED가 첫 번째 멤버여야 `reinterpret_cast` 가능 |
| **completionKey** | Client* 포인터 저장 → GQCS 반환 시 어떤 클라이언트인지 식별 |
| **비동기 체인** | Accept → Recv → Send → Recv → ... (연속적인 비동기 호출) |
| **재사용** | Accept 완료 후 PostAccept() 재호출, Recv/Send 완료 후 Recv 재호출 |

---

## 1. 클래스별 역할

| 클래스 | 역할 | 레이어 |
|--------|------|--------|
| **OverlappedExt** | 비동기 I/O 컨텍스트 (WSAOVERLAPPED 확장) | Network |
| **WindowsAsyncSocket** | Listen 소켓 생성/바인딩/리스닝 | Network |
| **Client** | 클라이언트 연결 상태 및 Overlapped 버퍼 관리 | Network |
| **IoCompletionPort** | IOCP 핸들 관리 및 Worker 스레드 풀 운영 | Network |
| **Acceptor** | AcceptEx 기반 비동기 연결 수락 | Network |
| **EchoServer** | 비즈니스 로직 (Echo 처리) 및 컴포넌트 조율 | Game Logic |

---

## 2. 클래스별 핵심 기능

### IoCompletionPort

```
┌─────────────────────────────────────────────────────────┐
│  IoCompletionPort                                       │
├─────────────────────────────────────────────────────────┤
│  책임: IOCP HANDLE 관리 + Worker 스레드 풀                 │
├─────────────────────────────────────────────────────────┤
│  Initialize(workerCount)                                │
│    └─ CreateIoCompletionPort() 호출                      │
│    └─ Worker 스레드 N개 생성                               │
│                                                         │
│  AssociateSocket(socket, completionKey)                 │
│    └─ 소켓을 IOCP에 연결                                  │
│    └─ completionKey = Client* (Recv/Send 식별용)         │
│                                                         │
│  WorkerLoop()  [private, 각 스레드에서 실행]               │
│    └─ GetQueuedCompletionStatus() 대기 (Blocking)        │
│    └─ GQCS() thread awake                               │
│    └─ CompletionEvent 생성 → Handler 콜백 호출            │
│                                                         │
│  SetCompletionHandler(callback)                         │
│    └─ 완료 이벤트 처리 콜백 등록                            │
└─────────────────────────────────────────────────────────┘
```

**주요 메서드:**
- `Initialize(int workerThreadCount)` - IOCP 생성 및 Worker 스레드 시작
- `AssociateSocket(SocketHandle, void* data)` - 소켓을 IOCP에 연결
- `SetCompletionHandler(CompletionHandler)` - 완료 이벤트 콜백 등록
- `Shutdown()` - Worker 스레드 종료 및 IOCP 핸들 해제

---

### Acceptor

```
┌─────────────────────────────────────────────────────────┐
│  Acceptor                                               │
├─────────────────────────────────────────────────────────┤
│  책임: AcceptEx 비동기 연결 수락                            │
├─────────────────────────────────────────────────────────┤
│  Initialize(listenSocket, iocpHandle)                   │
│    └─ Listen 소켓을 IOCP에 연결 (Accept 완료 통지용)        │
│    └─ WSAIoctl로 AcceptEx 함수 포인터 (간접 실행, 링킹 x)    │
│    └─ OverlappedExt Pool 사전 할당                        │
│                                                         │
│  PostAccept()                                           │
│    └─ Accept용 소켓 생성 (WSA_FLAG_OVERLAPPED)            │
│    └─ AcceptEx() 비동기 호출                              │
│                                                         │
│  OnAcceptComplete(overlapped, bytesTransferred)         │
│    └─ SO_UPDATE_ACCEPT_CONTEXT 설정                      │
│    └─ GetAcceptExSockAddrs로 주소 파싱                    │
│    └─ AcceptCallback 호출 → EchoServer에 알림            │
│    └─ PostAccept() 재호출 (다음 연결 대기)                 │
└─────────────────────────────────────────────────────────┘
```

**주요 메서드:**
- `Initialize(SocketHandle, HANDLE)` - Listen 소켓 IOCP 연결 및 AcceptEx 함수 로드
- `PostAccept()` - AcceptEx 비동기 호출
- `PostAccepts(int count)` - 여러 개의 AcceptEx 사전 호출
- `OnAcceptComplete(OverlappedExt*, DWORD recvBytes)` - Accept 완료 처리
- `SetAcceptCallback(AcceptCallback)` - 연결 수락 콜백 등록

---

### EchoServer

```
┌─────────────────────────────────────────────────────────┐
│  EchoServer                                             │
├─────────────────────────────────────────────────────────┤
│  책임: Echo 로직 + 컴포넌트 조율 테스트                      │
├─────────────────────────────────────────────────────────┤
│  Start(asyncSocket)                                     │
│    └─ IoCompletionPort 생성/초기화                        │
│    └─ Acceptor 생성/초기화 (IOCP 핸들 주입)                 │
│    └─ CompletionHandler 콜백 등록                         │
│    └─ AcceptCallback 콜백 등록                            │
│    └─ Client 풀 사전 할당                                 │
│    └─ PostAccepts() 호출                                 │
│                                                         │
│  OnCompletion(event)  [IOCP Worker에서 호출됨]            │
│    └─ Accept → Acceptor.OnAcceptComplete()              │
│    └─ Recv  → Echo 로직 (받은 데이터를 Send)               │
│    └─ Send  → 로깅만                                     │
│                                                         │
│  OnAccept(ctx)  [Acceptor 콜백]                          │
│    └─ Client 풀에서 빈 슬롯 찾기                           │
│    └─ Client 소켓을 생성해둔 IOCP Handle에 연결             │
│    └─ 첫 Recv 비동기 호출                                 │
│                                                         │
│  Recv(client) / Send(client, msg)                       │
│    └─ WSARecv / WSASend 비동기 호출                       │
└─────────────────────────────────────────────────────────┘
```

**주요 메서드:**
- `Start(std::shared_ptr<ISocket>)` - 서버 시작 (IOCP, Acceptor 초기화)
- `Stop()` - 서버 종료
- `OnCompletion(CompletionEvent&)` - I/O 완료 이벤트 처리 (Accept/Recv/Send)
- `OnAccept(AcceptContext&)` - 새 클라이언트 연결 처리
- `Recv(std::shared_ptr<Client>)` - 비동기 수신
- `Send(std::shared_ptr<Client>, std::string_view, ULONG)` - 비동기 송신

---

### Client & OverlappedExt

```
┌─────────────────────────────────────────────────────────┐
│  Client                                                 │
├─────────────────────────────────────────────────────────┤
│  socket          : 클라이언트 소켓 핸들                    │
│  recvOverlapped  : Recv용 OverlappedExt                 │
│  sendOverlapped  : Send용 OverlappedExt                 │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  OverlappedExt                                          │
├─────────────────────────────────────────────────────────┤
│  overlapped      : WSAOVERLAPPED (반드시 첫 번째!)        │
│  ioType          : Accept / Recv / Send 구분            │
│  clientSocket    : Accept 시 생성된 소켓                  │
│  wsaBuffer       : WSARecv/WSASend용 버퍼 디스크립터       │
│  buffer[8192]    : 실제 데이터 버퍼                        │
└─────────────────────────────────────────────────────────┘
```

---

### Worker Thread 내부 루프

```
┌─────────────────────────────────────────────────────────────────┐
│                    WorkerLoop (각 스레드)                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   while (!stop_requested && isRunning) {                        │
│       │                                                         │
│       ▼                                                         │
│   ┌─────────────────────────────────┐                           │
│   │ GetQueuedCompletionStatus()     │◄──── 블로킹 대기          │
│   │   - bytesTransferred            │                           │
│   │   - completionKey (Client*)     │                           │
│   │   - overlapped (OverlappedExt*) │                           │
│   └─────────────────────────────────┘                           │
│       │                                                         │
│       ▼                                                         │
│   ┌─────────────────────────────────┐                           │
│   │ CompletionEvent 생성            │                           │
│   │   - ioType = overlapped->ioType │                           │
│   │   - success, errorCode 설정     │                           │
│   └─────────────────────────────────┘                           │
│       │                                                         │
│       ▼                                                         │
│   ┌─────────────────────────────────┐                           │
│   │ _completionHandler(event)       │───► EchoServer::          │
│   │                                 │     OnCompletion()        │
│   └─────────────────────────────────┘                           │
│       │                                                         │
│       └──────────────► 루프 반복                                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. 파일 구조

```
highp-mmorpg/
├── network/
│   ├── IoCompletionPort.h/cpp    # IOCP 관리
│   ├── Acceptor.h/cpp            # AcceptEx 기반 연결 수락
│   ├── AcceptContext.h           # Accept 완료 정보
│   ├── OverlappedExt.h           # 확장 Overlapped 구조체
│   ├── Client.h/cpp              # 클라이언트 상태
│   ├── WindowsAsyncSocket.h/cpp  # Listen 소켓
│   ├── EIoType.h                 # I/O 타입 열거형
│   └── platform.h                # Windows 헤더
│
├── echo/
│   ├── echo-server/
│   │   ├── EchoServer.h/cpp      # Echo 서버 메인 로직
│   │   └── main.cpp              # 진입점
│   └── echo-client/
│       ├── EchoClient.h/cpp      # 테스트 클라이언트
│       └── main.cpp              # 진입점
│
└── lib/
    ├── Result.hpp                # Result 타입
    ├── Errors.hpp                # 에러 처리
    └── Logger.hpp                # 로깅
```

---

## 4. 주의사항

1. **OverlappedExt 구조체**
   - `WSAOVERLAPPED overlapped`가 반드시 첫 번째 멤버여야 함
   - 상속 사용 시 메모리 레이아웃 문제 발생 가능 -> 확장

2. **Listen 소켓 IOCP 연결**
   - AcceptEx 완료 통지를 받으려면 Listen 소켓도 IOCP에 연결 필요

3. **SO_UPDATE_ACCEPT_CONTEXT**
   - AcceptEx로 수락된 소켓은 반드시 이 옵션 설정 필요 (sockopt, 그렇지 않으면 listen() 에서 자동 accept 가 발생)

4. **비동기 I/O 재호출**
   - Recv/Send 완료 후 다음 Recv를 반드시 호출해야 함
   - Accept 완료 후 PostAccept()로 다음 연결 대기

5. **메모리 수명**
   - Overlapped 구조체는 I/O 완료 전까지 유효해야 함
   - Client 풀 또는 OverlappedExt 풀로 관리

# IOCP 실행 흐름 Sequence Diagrams

**프로젝트**: highp-mmorpg Echo Server
**작성일**: 2026-01-20
**목적**: AcceptEx, WSARecv, WSASend의 비동기 실행 흐름 시각화

---

## 📚 목차

1. [Accept 흐름](#1-accept-흐름)
2. [Recv 흐름](#2-recv-흐름)
3. [Send 흐름](#3-send-흐름)
4. [전체 Echo 사이클](#4-전체-echo-사이클)

---

## 1. Accept 흐름

### AcceptEx 비동기 연결 수락 프로세스

```mermaid
sequenceDiagram
    participant App as EchoServer
    participant Acceptor as Acceptor
    participant IOCP as IOCP Queue
    participant Kernel as Windows Kernel
    participant Worker as IOCP Worker Thread
    participant Client as Client

    Note over App: 서버 시작
    App->>Acceptor: Start() → PostAccepts(backlog)

    loop backlog 횟수만큼
        Acceptor->>Acceptor: CreateAcceptSocket()
        Acceptor->>Acceptor: AcquireOverlapped()
        Note over Acceptor: overlapped->ioType = Accept
        Acceptor->>Kernel: AcceptEx(listenSocket, acceptSocket, overlapped)
        Kernel-->>Acceptor: FALSE + ERROR_IO_PENDING (정상)
        Note over Kernel: 비동기 작업 시작<br/>클라이언트 연결 대기 중
    end

    Note over Kernel,IOCP: 클라이언트 연결 시도 발생
    Kernel->>IOCP: 완료 패킷 삽입<br/>(bytes=0, key=0, overlapped)

    IOCP->>Worker: GetQueuedCompletionStatus()<br/>스레드 깨움
    Worker->>Worker: event.ioType == Accept 확인

    Worker->>App: OnCompletion(event)
    App->>Acceptor: OnAcceptComplete(overlapped, bytes)

    Note over Acceptor: SO_UPDATE_ACCEPT_CONTEXT 설정
    Acceptor->>Acceptor: setsockopt(acceptSocket, SO_UPDATE_ACCEPT_CONTEXT)
    Acceptor->>Acceptor: GetAcceptExSockaddrs() - 주소 파싱

    Acceptor->>App: OnAccept(AcceptContext)

    Note over App: 클라이언트 풀에서 할당
    App->>App: FindAvailableClient()
    App->>App: client->socket = acceptSocket

    App->>IOCP: AssociateSocket(client->socket, client)
    Note over IOCP: 클라이언트 소켓을<br/>IOCP에 연결

    App->>Client: PostRecv()
    Note over Client: 첫 수신 대기 시작

    Note over Acceptor: 다음 Accept 준비
    Acceptor->>Acceptor: ReleaseOverlapped()
    Acceptor->>Kernel: PostAccept() - 새 AcceptEx 등록

    Note over App,Client: ✅ 클라이언트 연결 완료<br/>수신 대기 상태
```

### 코드 경로
```
EchoServer::Start() [EchoServer.cpp:22]
  └─> Acceptor::PostAccepts() [Acceptor.cpp:146]
      └─> Acceptor::PostAccept() [Acceptor.cpp:104]
          └─> AcceptEx() [Acceptor.cpp:123]

IOCP Worker Thread
  └─> GetQueuedCompletionStatus() [IoCompletionPort.cpp:100]
      └─> OnCompletion() [IoCompletionPort.cpp:124]
          └─> EchoServer::OnCompletion() [EchoServer.cpp:74]
              └─> Acceptor::OnAcceptComplete() [Acceptor.cpp:156]
                  └─> EchoServer::OnAccept() [EchoServer.cpp:128]
                      └─> Client::PostRecv() [Client.cpp:12]
```

---

## 2. Recv 흐름

### WSARecv 비동기 데이터 수신 프로세스

```mermaid
sequenceDiagram
    participant App as EchoServer
    participant Client as Client
    participant IOCP as IOCP Queue
    participant Kernel as Windows Kernel
    participant Worker as IOCP Worker Thread

    Note over Client: 수신 대기 시작
    App->>Client: PostRecv()

    Note over Client: Overlapped 구조체 준비
    Client->>Client: ZeroMemory(&recvOverlapped)
    Client->>Client: recvOverlapped.ioType = Recv
    Client->>Client: wsaBuffer.buf = recvBuffer (4096 bytes)

    Client->>Kernel: WSARecv(socket, &wsaBuffer, &recvOverlapped)
    Kernel-->>Client: SOCKET_ERROR + WSA_IO_PENDING (정상)

    Note over Kernel,IOCP: 클라이언트가 데이터 전송
    Note over Kernel: 커널 버퍼에 데이터 도착<br/>recvBuffer로 복사

    Kernel->>IOCP: 완료 패킷 삽입<br/>(bytes=전송량, key=Client*, overlapped)

    IOCP->>Worker: GetQueuedCompletionStatus()<br/>스레드 깨움

    Worker->>Worker: event.ioType == Recv 확인
    Worker->>Worker: event.success == TRUE 확인
    Worker->>Worker: bytesTransferred > 0 확인

    Worker->>App: OnCompletion(event)

    Note over App: 수신 데이터 처리
    App->>App: client = event.completionKey
    App->>App: overlapped = event.overlapped
    App->>App: recvBuffer[bytesTransferred] = '\0'
    App->>App: string_view recvData(recvBuffer, bytes)

    Note over App: Echo: 받은 데이터 그대로 전송
    App->>Client: PostSend(recvData)
    Note over Client: WSASend 시작 (다음 다이어그램)

    App->>Client: PostRecv()
    Note over Client: 다음 수신 대기

    Note over App,Client: ✅ Recv 완료<br/>Echo Send 시작<br/>다음 Recv 대기 중
```

### 특수 케이스: Graceful Disconnect

```mermaid
sequenceDiagram
    participant App as EchoServer
    participant Client as Client
    participant IOCP as IOCP Queue
    participant Kernel as Windows Kernel
    participant Worker as IOCP Worker Thread

    Note over Kernel: 클라이언트가 연결 종료<br/>(shutdown 또는 closesocket)

    Kernel->>IOCP: 완료 패킷 삽입<br/>(bytes=0, key=Client*, overlapped)

    IOCP->>Worker: GetQueuedCompletionStatus()
    Worker->>Worker: bytesTransferred == 0 확인

    Worker->>App: OnCompletion(event)

    App->>App: bytesTransferred == 0 감지
    Note over App: Graceful Disconnect
    App->>Client: CloseClient(client)
    Client->>Kernel: shutdown(SD_BOTH)
    Client->>Kernel: closesocket(socket)

    Note over App,Client: ✅ 클라이언트 정상 종료
```

### 코드 경로
```
Client::PostRecv() [Client.cpp:12]
  └─> WSARecv() [Client.cpp:22]

IOCP Worker Thread
  └─> GetQueuedCompletionStatus() [IoCompletionPort.cpp:100]
      └─> EchoServer::OnCompletion() [EchoServer.cpp:74]
          └─> case EIoType::Recv [EchoServer.cpp:86]
              ├─> Client::PostSend() [EchoServer.cpp:104]
              └─> Client::PostRecv() [EchoServer.cpp:109]
```

---

## 3. Send 흐름

### WSASend 비동기 데이터 송신 프로세스

```mermaid
sequenceDiagram
    participant App as EchoServer
    participant Client as Client
    participant IOCP as IOCP Queue
    participant Kernel as Windows Kernel
    participant Worker as IOCP Worker Thread

    Note over Client: 송신 시작
    App->>Client: PostSend(data)

    Note over Client: Overlapped 구조체 준비
    Client->>Client: ZeroMemory(&sendOverlapped)
    Client->>Client: sendOverlapped.ioType = Send
    Client->>Client: CopyMemory(sendBuffer, data, len)
    Client->>Client: wsaBuffer.buf = sendBuffer
    Client->>Client: wsaBuffer.len = len (최대 1024 bytes)

    Client->>Kernel: WSASend(socket, &wsaBuffer, &sendOverlapped)
    Kernel-->>Client: SOCKET_ERROR + WSA_IO_PENDING (정상)

    Note over Kernel: 커널이 sendBuffer 데이터를<br/>네트워크로 전송 시작

    Note over Kernel,IOCP: 전송 완료 (TCP ACK 수신)

    Kernel->>IOCP: 완료 패킷 삽입<br/>(bytes=전송량, key=Client*, overlapped)

    IOCP->>Worker: GetQueuedCompletionStatus()<br/>스레드 깨움

    Worker->>Worker: event.ioType == Send 확인
    Worker->>Worker: event.success == TRUE 확인

    Worker->>App: OnCompletion(event)

    Note over App: 송신 완료 처리 (현재는 로깅만)
    App->>App: client = event.completionKey
    App->>App: Log: "Send socket #, bytes"

    Note over App,Client: ✅ Send 완료<br/>sendBuffer 재사용 가능
```

### 코드 경로
```
Client::PostSend() [Client.cpp:38]
  └─> WSASend() [Client.cpp:50]

IOCP Worker Thread
  └─> GetQueuedCompletionStatus() [IoCompletionPort.cpp:100]
      └─> EchoServer::OnCompletion() [EchoServer.cpp:74]
          └─> case EIoType::Send [EchoServer.cpp:115]
              └─> 로깅만 수행
```

---

## 4. 전체 Echo 사이클

### 클라이언트 연결부터 Echo 응답까지 전체 흐름

```mermaid
sequenceDiagram
    participant Client as TCP Client
    participant Kernel as Windows Kernel
    participant IOCP as IOCP Queue
    participant Worker as Worker Thread
    participant App as EchoServer
    participant AcceptorObj as Acceptor
    participant ClientObj as Client Object

    Note over Client,ClientObj: 🔵 Phase 1: Accept
    Client->>Kernel: TCP SYN (연결 요청)
    Kernel->>Kernel: AcceptEx 완료
    Kernel->>IOCP: Accept 완료 패킷
    IOCP->>Worker: Wake up
    Worker->>App: OnCompletion(Accept)
    App->>AcceptorObj: OnAcceptComplete()
    AcceptorObj->>App: OnAccept()
    App->>ClientObj: new Client
    App->>IOCP: AssociateSocket(client)
    App->>ClientObj: PostRecv() #1
    ClientObj->>Kernel: WSARecv() #1

    Note over Client,ClientObj: ✅ 연결 완료, 수신 대기 중

    Note over Client,ClientObj: 🔵 Phase 2: Recv
    Client->>Kernel: TCP Data "Hello"
    Kernel->>Kernel: recvBuffer에 복사
    Kernel->>IOCP: Recv #1 완료 패킷 (5 bytes)
    IOCP->>Worker: Wake up
    Worker->>App: OnCompletion(Recv)
    App->>App: 데이터 읽기: "Hello"
    App->>App: Log: "[Recv] Hello, 5 bytes"

    Note over Client,ClientObj: 🔵 Phase 3: Send (Echo)
    App->>ClientObj: PostSend("Hello")
    ClientObj->>Kernel: WSASend()
    Kernel->>Kernel: sendBuffer 전송
    Kernel->>Client: TCP Data "Hello"

    Note over Client,ClientObj: 🔵 Phase 4: Send 완료
    Kernel->>IOCP: Send 완료 패킷 (5 bytes)
    IOCP->>Worker: Wake up
    Worker->>App: OnCompletion(Send)
    App->>App: Log: "[Send] 5 bytes"

    Note over Client,ClientObj: 🔵 Phase 5: 다음 Recv 대기
    App->>ClientObj: PostRecv() #2
    ClientObj->>Kernel: WSARecv() #2

    Note over Client,ClientObj: ✅ Echo 완료<br/>다음 메시지 대기 중

    Note over Client,ClientObj: 🔵 Phase 6: Graceful Disconnect
    Client->>Kernel: TCP FIN (연결 종료)
    Kernel->>IOCP: Recv #2 완료 패킷 (0 bytes)
    IOCP->>Worker: Wake up
    Worker->>App: OnCompletion(Recv)
    App->>App: bytesTransferred == 0 감지
    App->>ClientObj: CloseClient()
    ClientObj->>Kernel: shutdown() + closesocket()

    Note over Client,ClientObj: ✅ 클라이언트 연결 종료
```

---

## 5. IOCP Worker Thread 메인 루프

### WorkerLoop의 지속적인 완료 패킷 처리

```mermaid
sequenceDiagram
    participant Worker as Worker Thread
    participant IOCP as IOCP Queue
    participant Kernel as Windows Kernel
    participant App as EchoServer

    Note over Worker: 워커 스레드 시작

    loop 무한 루프 (stop 요청까지)
        Worker->>IOCP: GetQueuedCompletionStatus(INFINITE)
        Note over Worker,IOCP: ⏸️ 블로킹 대기<br/>(완료 패킷이 올 때까지)

        alt 완료 패킷 도착
            IOCP-->>Worker: (bytes, key, overlapped)

            alt Shutdown 신호 (key=0, overlapped=NULL)
                Worker->>Worker: break - 루프 종료
            else Accept 완료
                Worker->>App: OnCompletion(Accept)
            else Recv 완료
                Worker->>App: OnCompletion(Recv)
            else Send 완료
                Worker->>App: OnCompletion(Send)
            end
        end
    end

    Note over Worker: 워커 스레드 종료
```

---

## 6. 핵심 개념 정리

### 6.1 비동기 I/O의 ERROR_IO_PENDING

```mermaid
sequenceDiagram
    participant App as Application
    participant Func as WSARecv/WSASend/AcceptEx
    participant Kernel as Kernel

    App->>Func: 비동기 함수 호출

    alt 즉시 완료 (드문 경우)
        Func-->>App: 성공 (0 또는 TRUE)
        Note over App: 동기적 완료<br/>IOCP 통지 없음
    else 비동기 진행 (일반적)
        Func->>Kernel: I/O 작업 등록
        Func-->>App: SOCKET_ERROR + WSA_IO_PENDING
        Note over App: ⚠️ 에러가 아님!<br/>정상적인 비동기 시작

        Note over Kernel: I/O 작업 진행 중...

        Kernel->>Kernel: I/O 완료
        Kernel->>App: IOCP 완료 통지
        Note over App: GetQueuedCompletionStatus에서<br/>완료 패킷 수신
    end
```

### 6.2 Overlapped 구조체의 역할

```
┌─────────────────────────────────────────┐
│ Client 객체                              │
├─────────────────────────────────────────┤
│ SOCKET socket                            │
│                                          │
│ OverlappedExt recvOverlapped ───────┐   │
│   ├─ WSAOVERLAPPED overlapped       │   │
│   ├─ EIoType ioType = Recv          │   │
│   ├─ char recvBuffer[4096]          │   │
│   └─ WSABUF wsaBuffer               │   │
│                                     │   │
│ OverlappedExt sendOverlapped ───────┼─┐ │
│   ├─ WSAOVERLAPPED overlapped       │ │ │
│   ├─ EIoType ioType = Send          │ │ │
│   ├─ char sendBuffer[1024]          │ │ │
│   └─ WSABUF wsaBuffer               │ │ │
└─────────────────────────────────────┼─┼─┘
                                      │ │
        WSARecv(..., &recvOverlapped) ┘ │
        WSASend(..., &sendOverlapped) ──┘
                      ↓
              Windows Kernel이
              overlapped 주소 기억
                      ↓
              I/O 완료 시
              IOCP 큐에 삽입
                      ↓
     GetQueuedCompletionStatus(&overlapped)
                      ↓
         (OverlappedExt*)overlapped
              ioType 확인!
```

### 6.3 CompletionKey의 역할

```
AssociateSocket(socket, client_ptr)
                    ↓
        client_ptr을 completionKey로 등록
                    ↓
       WSARecv/WSASend 호출 시
    커널이 completionKey를 기억
                    ↓
            I/O 완료 시
  IOCP에 completionKey 포함하여 통지
                    ↓
GetQueuedCompletionStatus(&completionKey)
                    ↓
    (Client*)completionKey
      Client 객체 복원!
```

---

## 7. 에러 처리 흐름

### GetQueuedCompletionStatus 에러 분기

```mermaid
sequenceDiagram
    participant Worker as Worker Thread
    participant IOCP as IOCP
    participant App as EchoServer

    Worker->>IOCP: GetQueuedCompletionStatus()

    alt ok == TRUE
        IOCP-->>Worker: success=TRUE, bytes, key, overlapped

        alt bytesTransferred == 0
            Worker->>App: OnCompletion(Recv)
            App->>App: Graceful Disconnect
            Note over App: 클라이언트 정상 종료
        else bytesTransferred > 0
            Worker->>App: OnCompletion(Recv/Send)
            Note over App: 정상 처리
        end

    else ok == FALSE && overlapped == NULL
        IOCP-->>Worker: GQCS 함수 자체 실패

        alt error == ERROR_ABANDONED_WAIT_0
            Note over Worker: IOCP 핸들 닫힘<br/>→ 워커 스레드 종료
        else error == WAIT_TIMEOUT
            Note over Worker: 타임아웃<br/>→ 재시도
        end

    else ok == FALSE && overlapped != NULL
        IOCP-->>Worker: I/O 작업 실패

        alt error == ERROR_NETNAME_DELETED
            Note over Worker: 원격 연결 종료
            Worker->>App: CloseClient()
        else error == ERROR_OPERATION_ABORTED
            Note over Worker: I/O 취소됨 (CancelIoEx)
        else error == WSAECONNRESET
            Note over Worker: 연결 강제 종료
            Worker->>App: CloseClient()
        end
    end
```

---

## 8. 참고: 코드 위치

### 주요 파일 및 함수

| 컴포넌트 | 파일 경로 | 주요 함수 |
|---------|----------|----------|
| **EchoServer** | echo/echo-server/EchoServer.cpp | Start(), OnCompletion(), OnAccept() |
| **Acceptor** | network/Acceptor.cpp | PostAccept(), OnAcceptComplete() |
| **Client** | network/Client.cpp | PostRecv(), PostSend() |
| **IoCompletionPort** | network/IoCompletionPort.cpp | WorkerLoop(), Initialize() |
| **OverlappedExt** | network/OverlappedExt.h | 구조체 정의 |

### 주요 상수

| 상수 | 값 | 정의 위치 | 설명 |
|------|-----|----------|------|
| recvBufferSize | 4096 | network/Const.h | 수신 버퍼 크기 |
| sendBufferSize | 1024 | network/Const.h | 송신 버퍼 크기 |
| backlog | 설정값 | config.runtime.toml | 동시 Accept 개수 |
| maxWorkerThreadMultiplier | 설정값 | config.runtime.toml | 워커 스레드 배수 |

---

**문서 버전**: 1.0
**최종 수정**: 2026-01-20
**작성자**: Claude Code Analysis Team

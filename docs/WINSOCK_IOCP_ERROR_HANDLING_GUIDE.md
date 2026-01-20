# Windows Sockets & IOCP 함수 완벽 가이드

**작성일**: 2026-01-20
**목적**: Windows IOCP 기반 MMORPG 서버 개발을 위한 학습 자료

---

## 📚 목차

1. [개요](#개요)
2. [함수 분류](#함수-분류)
3. [소켓 함수 상세](#소켓-함수-상세)
   - [WSASocket](#wsasocket)
   - [bind](#bind)
   - [listen](#listen)
   - [AcceptEx](#acceptex)
   - [setsockopt](#setsockopt)
   - [WSARecv](#wsarecv)
   - [WSASend](#wsasend)
   - [WSAIoctl](#wsaioctl)
4. [IOCP 함수 상세](#iocp-함수-상세)
   - [CreateIoCompletionPort](#createiocompletionport)
   - [GetQueuedCompletionStatus](#getqueuedcompletionstatus)
   - [GetQueuedCompletionStatusEx](#getqueuedcompletionstatusex)
   - [PostQueuedCompletionStatus](#postqueuedcompletionstatus)
   - [CancelIoEx](#cancelioex)
5. [함수 비교표](#함수-비교표)
6. [실전 패턴](#실전-패턴)
7. [참고 자료](#참고-자료)

---

## 개요

이 문서는 Windows IOCP 기반 고성능 네트워크 서버 개발에 필요한 핵심 Winsock 및 IOCP 함수들의 동작 방식, 에러 코드, 핸들링 전략을 정리한 학습 자료입니다.

### 문서 범위

- **소켓 생성 및 설정**: WSASocket, bind, listen, setsockopt
- **비동기 I/O**: AcceptEx, WSARecv, WSASend
- **IOCP 관리**: CreateIoCompletionPort, GetQueuedCompletionStatus/Ex, PostQueuedCompletionStatus
- **제어 및 취소**: WSAIoctl, CancelIoEx

### 용어 정의

| 용어 | 설명 |
|------|------|
| **Synchronous** | 함수가 작업 완료까지 호출 스레드를 블로킹 |
| **Asynchronous** | 함수가 즉시 반환하고 작업을 백그라운드에서 수행 |
| **Blocking** | 작업이 완료될 때까지 스레드가 대기 상태 |
| **Non-blocking** | 즉시 반환 (완료되지 않아도) |
| **Overlapped I/O** | Windows의 비동기 I/O 메커니즘 |
| **IOCP** | I/O Completion Port - 고성능 비동기 I/O 처리 커널 객체 |

---

## 함수 분류

### 카테고리별 분류

```
소켓 생성 및 초기화
├─ WSASocket        (소켓 생성)
├─ bind             (로컬 주소 바인딩)
└─ listen           (연결 대기 시작)

비동기 I/O 작업
├─ AcceptEx         (비동기 연결 수락)
├─ WSARecv          (비동기 데이터 수신)
└─ WSASend          (비동기 데이터 송신)

소켓 제어
├─ setsockopt       (소켓 옵션 설정)
└─ WSAIoctl         (소켓 I/O 제어, 확장 함수 획득)

IOCP 관리
├─ CreateIoCompletionPort  (IOCP 생성 및 소켓 연결)
├─ GetQueuedCompletionStatus   (완료 패킷 가져오기 - 단일)
└─ GetQueuedCompletionStatusEx (완료 패킷 가져오기 - 배치)

I/O 취소
└─ CancelIoEx       (비동기 I/O 취소)
```

### 동작 모드별 분류

| 함수 | Synchronous/Asynchronous | Blocking/Non-blocking |
|------|--------------------------|----------------------|
| WSASocket | Synchronous | Non-blocking (생성은 즉시) |
| bind | Synchronous | Non-blocking (바인딩은 즉시) |
| listen | Synchronous | Non-blocking (리슨 시작은 즉시) |
| setsockopt | Synchronous | Non-blocking (옵션 설정은 즉시) |
| **AcceptEx** | **Asynchronous** | **Non-blocking** (즉시 반환, IOCP 완료 통지) |
| **WSARecv** | **Asynchronous** (Overlapped) | **Non-blocking** (ERROR_IO_PENDING 반환) |
| **WSASend** | **Asynchronous** (Overlapped) | **Non-blocking** (ERROR_IO_PENDING 반환) |
| WSAIoctl | Both (파라미터 의존) | Both (Overlapped 사용 시 비동기) |
| CreateIoCompletionPort | Synchronous | Non-blocking (즉시 생성) |
| **GetQueuedCompletionStatus** | **Synchronous** | **Blocking** (타임아웃까지 대기) |
| **GetQueuedCompletionStatusEx** | **Synchronous** | **Blocking** (타임아웃까지 대기) |
| CancelIoEx | Synchronous | Non-blocking (취소 요청만, 완료는 비동기) |

---

## 소켓 함수 상세

### WSASocket

#### 함수 시그니처
```cpp
SOCKET WSASocket(
    int af,                        // 주소 패밀리 (AF_INET, AF_INET6)
    int type,                      // 소켓 타입 (SOCK_STREAM, SOCK_DGRAM)
    int protocol,                  // 프로토콜 (IPPROTO_TCP, IPPROTO_UDP)
    LPWSAPROTOCOL_INFO lpProtocolInfo,  // 프로토콜 정보 (보통 NULL)
    GROUP g,                       // 소켓 그룹 (보통 0)
    DWORD dwFlags                  // 소켓 속성 플래그
);
```

#### 역할
- Winsock 2의 확장 소켓 생성 함수
- **Overlapped I/O 지원**: `WSA_FLAG_OVERLAPPED` 플래그로 비동기 I/O 가능 소켓 생성
- 표준 `socket()` 함수보다 강력한 기능 제공

#### 동작 특성
- **Synchronous**: 소켓 생성은 즉시 완료
- **Non-blocking**: 호출 즉시 반환 (소켓 핸들 또는 INVALID_SOCKET)

#### 주요 에러 코드

| 에러 코드 | 값 | 발생 조건 | 복구 가능 |
|----------|-----|----------|-----------|
| WSANOTINITIALISED | 10093 | WSAStartup 미호출 | ✅ WSAStartup 호출 후 재시도 |
| WSAENETDOWN | 10050 | 네트워크 서브시스템 실패 | ❌ 시스템 재시작 필요 |
| WSAEAFNOSUPPORT | 10047 | 지원하지 않는 주소 패밀리 | ❌ 파라미터 수정 필요 |
| WSAEMFILE | 10024 | 소켓 디스크립터 한계 도달 | ⚠️ 기존 소켓 닫고 재시도 |
| WSAENOBUFS | 10055 | 시스템 버퍼 부족 | ⚠️ 일시적, 재시도 가능 |
| WSAEPROTONOSUPPORT | 10043 | 지원하지 않는 프로토콜 | ❌ 파라미터 수정 필요 |

#### 에러 핸들링 예시

```cpp
SOCKET socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                          nullptr, 0, WSA_FLAG_OVERLAPPED);
if (socket == INVALID_SOCKET) {
    DWORD error = WSAGetLastError();
    switch (error) {
        case WSANOTINITIALISED:
            // WSAStartup 호출 후 재시도
            break;
        case WSAEMFILE:
            // 기존 소켓 정리 후 재시도
            break;
        case WSAENOBUFS:
            // 일시적 문제, 짧은 대기 후 재시도
            Sleep(100);
            break;
        default:
            // 치명적 에러
            break;
    }
}
```

---

### bind

#### 함수 시그니처
```cpp
int bind(
    SOCKET s,                     // 소켓 핸들
    const sockaddr *name,         // 바인딩할 로컬 주소
    int namelen                   // 주소 구조체 크기
);
```

#### 역할
- 소켓을 로컬 IP 주소와 포트에 바인딩
- 서버 소켓 생성 시 필수
- 클라이언트는 보통 생략 (자동 할당)

#### 동작 특성
- **Synchronous**: 바인딩 작업이 즉시 완료
- **Non-blocking**: 호출 즉시 반환 (성공 또는 실패)

#### 주요 에러 코드

| 에러 코드 | 값 | 발생 조건 | 복구 가능 |
|----------|-----|----------|-----------|
| WSAEADDRINUSE | 10048 | 포트가 이미 사용 중 | ⚠️ 다른 포트 사용 또는 SO_REUSEADDR 설정 |
| WSAEADDRNOTAVAIL | 10049 | 유효하지 않은 로컬 IP | ❌ IP 주소 수정 필요 |
| WSAEACCES | 10013 | 권한 거부 (브로드캐스트 등) | ❌ SO_BROADCAST 설정 또는 권한 상승 |
| WSAEINVAL | 10022 | 이미 바인딩된 소켓 | ❌ 프로그래밍 오류 |
| WSAENOBUFS | 10055 | 버퍼 공간 부족 (포트 고갈) | ⚠️ 일시적, 재시도 가능 |

#### 에러 핸들링 예시

```cpp
SOCKADDR_IN addr = {};
addr.sin_family = AF_INET;
addr.sin_port = htons(8080);
addr.sin_addr.s_addr = INADDR_ANY;

if (bind(socket, (SOCKADDR*)&addr, sizeof(addr)) != 0) {
    DWORD error = WSAGetLastError();
    switch (error) {
        case WSAEADDRINUSE:
            _logger->Error("Port 8080 is already in use");
            // SO_REUSEADDR 설정 시도 또는 다른 포트 사용
            break;
        case WSAEADDRNOTAVAIL:
            _logger->Error("Invalid IP address for binding");
            break;
        default:
            _logger->Error("bind failed: {}", error);
            break;
    }
}
```

---

### listen

#### 함수 시그니처
```cpp
int listen(
    SOCKET s,                     // 소켓 핸들
    int backlog                   // 대기 큐 크기
);
```

#### 역할
- 소켓을 연결 대기 상태로 전환
- 클라이언트 연결 요청을 backlog 크기만큼 큐잉
- `bind()` 호출 후 사용

#### 동작 특성
- **Synchronous**: 리슨 상태 전환이 즉시 완료
- **Non-blocking**: 호출 즉시 반환

#### 주요 에러 코드

| 에러 코드 | 값 | 발생 조건 | 복구 가능 |
|----------|-----|----------|-----------|
| WSAEADDRINUSE | 10048 | 같은 주소에 이미 리슨 중 | ❌ 포트 충돌 |
| WSAEINVAL | 10022 | 소켓이 bind되지 않음 | ❌ bind 호출 후 재시도 |
| WSAEISCONN | 10056 | 이미 연결된 소켓 | ❌ 프로그래밍 오류 |
| WSAEOPNOTSUPP | 10045 | SOCK_DGRAM 등 비지원 타입 | ❌ 소켓 타입 오류 |

#### 에러 핸들링 예시

```cpp
if (listen(socket, SOMAXCONN) != 0) {
    DWORD error = WSAGetLastError();
    switch (error) {
        case WSAEINVAL:
            _logger->Error("Socket not bound before listen");
            break;
        case WSAEISCONN:
            _logger->Error("Socket is already connected");
            break;
        case WSAEOPNOTSUPP:
            _logger->Error("listen not supported on this socket type");
            break;
    }
}
```

---

### AcceptEx

#### 함수 시그니처
```cpp
BOOL AcceptEx(
    SOCKET sListenSocket,                // 리슨 소켓
    SOCKET sAcceptSocket,                // 미리 생성된 accept 소켓
    PVOID lpOutputBuffer,                // 주소 저장 버퍼
    DWORD dwReceiveDataLength,           // 초기 데이터 수신 크기 (보통 0)
    DWORD dwLocalAddressLength,          // 로컬 주소 길이
    DWORD dwRemoteAddressLength,         // 원격 주소 길이
    LPDWORD lpdwBytesReceived,           // 수신 바이트 수 (Overlapped 시 무시)
    LPOVERLAPPED lpOverlapped            // Overlapped 구조체
);
```

#### 역할
- Windows 전용 고성능 비동기 연결 수락 함수
- `WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER, WSAID_ACCEPTEX)`로 함수 포인터 획득
- Accept 소켓을 미리 생성하여 성능 최적화
- **IOCP와 함께 사용하여 비동기 처리**

#### 동작 특성
- **Asynchronous**: 즉시 반환, IOCP 완료 통지
- **Non-blocking**: `ERROR_IO_PENDING` 반환 시 정상 (작업 진행 중)

#### 주요 에러 코드

| 에러 코드 | 값 | 의미 | 복구 가능 |
|----------|-----|------|-----------|
| **ERROR_IO_PENDING** | 997 | **정상 - 비동기 작업 진행 중** | ✅ 정상 케이스 |
| WSAECONNRESET | 10054 | 클라이언트가 수락 전에 연결 종료 | ✅ 재시도 (PostAccept 다시 호출) |
| WSAEINVAL | 10022 | 잘못된 파라미터 | ❌ 파라미터 검증 필요 |
| WSAENOTSOCK | 10038 | 유효하지 않은 소켓 | ❌ 소켓 재생성 필요 |
| WSAENOBUFS | 10055 | 시스템 버퍼 부족 | ⚠️ 일시적, 재시도 |

#### 에러 핸들링 예시

```cpp
BOOL result = AcceptEx(listenSocket, acceptSocket, buffer, 0,
                       addrLen, addrLen, &bytesReceived, &overlapped);
if (result == FALSE) {
    DWORD error = WSAGetLastError();

    if (error == ERROR_IO_PENDING) {
        // 정상 - IOCP 완료 대기
        return SUCCESS;
    }

    switch (error) {
        case WSAECONNRESET:
            // 클라이언트 조기 종료 - 재시도
            _logger->Warn("Client disconnected before accept");
            closesocket(acceptSocket);
            return PostAccept();  // 재시도

        case WSAENOBUFS:
            // 일시적 버퍼 부족 - 재시도
            Sleep(10);
            return PostAccept();

        default:
            _logger->Error("AcceptEx failed: {}", error);
            closesocket(acceptSocket);
            return ERROR;
    }
}
```

---

### setsockopt

#### 함수 시그니처
```cpp
int setsockopt(
    SOCKET s,                     // 소켓 핸들
    int level,                    // 옵션 레벨 (SOL_SOCKET, IPPROTO_TCP)
    int optname,                  // 옵션 이름 (SO_REUSEADDR, SO_KEEPALIVE 등)
    const char *optval,           // 옵션 값 포인터
    int optlen                    // 옵션 값 크기
);
```

#### 역할
- 소켓 옵션 설정
- **SO_UPDATE_ACCEPT_CONTEXT**: AcceptEx 후 필수 설정
- SO_KEEPALIVE, SO_REUSEADDR, TCP_NODELAY 등 다양한 옵션 제공

#### 동작 특성
- **Synchronous**: 옵션 설정이 즉시 완료
- **Non-blocking**: 호출 즉시 반환

#### 주요 에러 코드

| 에러 코드 | 값 | 발생 조건 | 복구 가능 |
|----------|-----|----------|-----------|
| WSAEINVAL | 10022 | 잘못된 옵션 값 또는 이미 연결된 소켓 | ❌ 파라미터 검증 필요 |
| WSAENOPROTOOPT | 10042 | 지원하지 않는 옵션 | ❌ 다른 옵션 사용 |
| WSAENOTSOCK | 10038 | 유효하지 않은 소켓 | ❌ 소켓 재생성 필요 |
| WSAEFAULT | 10014 | 잘못된 포인터 | ❌ 포인터 검증 필요 |

#### 에러 핸들링 예시 (SO_UPDATE_ACCEPT_CONTEXT)

```cpp
// AcceptEx 완료 후 필수 설정
int result = setsockopt(acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        (char*)&listenSocket, sizeof(listenSocket));
if (result == SOCKET_ERROR) {
    DWORD error = WSAGetLastError();
    switch (error) {
        case WSAENOTSOCK:
            _logger->Error("Invalid accept socket handle");
            break;
        case WSAEINVAL:
            _logger->Error("Listen socket handle invalid or socket already connected");
            break;
        default:
            _logger->Error("setsockopt(SO_UPDATE_ACCEPT_CONTEXT) failed: {}", error);
            break;
    }
    closesocket(acceptSocket);
}
```

---

### WSARecv

#### 함수 시그니처
```cpp
int WSARecv(
    SOCKET s,                           // 소켓 핸들
    LPWSABUF lpBuffers,                 // 버퍼 배열
    DWORD dwBufferCount,                // 버퍼 개수
    LPDWORD lpNumberOfBytesRecvd,       // 수신 바이트 수 (Overlapped 시 무시 가능)
    LPDWORD lpFlags,                    // 수신 플래그
    LPWSAOVERLAPPED lpOverlapped,       // Overlapped 구조체
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine  // 완료 루틴 (보통 NULL)
);
```

#### 역할
- 비동기 데이터 수신 함수
- **Overlapped I/O + IOCP 조합**으로 고성능 수신 처리

#### 동작 특성
- **Asynchronous**: Overlapped 사용 시 즉시 반환
- **Non-blocking**: `ERROR_IO_PENDING` 반환 시 정상

#### 주요 에러 코드

| 에러 코드 | 값 | 의미 | 복구 가능 |
|----------|-----|------|-----------|
| **WSA_IO_PENDING** | 997 | **정상 - 비동기 작업 진행 중** | ✅ 정상 케이스 |
| WSAECONNRESET | 10054 | 원격에서 연결 강제 종료 | ✅ 연결 종료 처리 |
| WSAECONNABORTED | 10053 | 로컬에서 연결 중단 | ✅ 연결 종료 처리 |
| WSAEDISCON | 10101 | 정상 연결 종료 (Graceful shutdown) | ✅ 연결 종료 처리 |
| WSAENOBUFS | 10055 | 시스템 버퍼 부족 | ⚠️ 일시적, 재시도 |
| WSAENOTCONN | 10057 | 소켓이 연결되지 않음 | ❌ 연결 상태 검증 필요 |

#### 에러 핸들링 예시

```cpp
int result = WSARecv(socket, &wsaBuf, 1, nullptr, &flags, &overlapped, nullptr);
if (result == SOCKET_ERROR) {
    DWORD error = WSAGetLastError();

    if (error == WSA_IO_PENDING) {
        // 정상 - IOCP 완료 대기
        return SUCCESS;
    }

    switch (error) {
        case WSAECONNRESET:
        case WSAECONNABORTED:
        case WSAEDISCON:
            // 연결 종료 - 클라이언트 정리
            _logger->Info("Client disconnected: {}", error);
            CloseClient(client);
            break;

        case WSAENOBUFS:
            // 일시적 버퍼 부족
            _logger->Warn("System buffer shortage, retry later");
            break;

        default:
            _logger->Error("WSARecv failed: {}", error);
            CloseClient(client);
            break;
    }
}
```

---

### WSASend

#### 함수 시그니처
```cpp
int WSASend(
    SOCKET s,                           // 소켓 핸들
    LPWSABUF lpBuffers,                 // 버퍼 배열
    DWORD dwBufferCount,                // 버퍼 개수
    LPDWORD lpNumberOfBytesSent,        // 송신 바이트 수 (Overlapped 시 무시 가능)
    DWORD dwFlags,                      // 송신 플래그
    LPWSAOVERLAPPED lpOverlapped,       // Overlapped 구조체
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine  // 완료 루틴
);
```

#### 역할
- 비동기 데이터 송신 함수
- **Overlapped I/O + IOCP 조합**으로 고성능 송신 처리

#### 동작 특성
- **Asynchronous**: Overlapped 사용 시 즉시 반환
- **Non-blocking**: `ERROR_IO_PENDING` 반환 시 정상

#### 주요 에러 코드

| 에러 코드 | 값 | 의미 | 복구 가능 |
|----------|-----|------|-----------|
| **WSA_IO_PENDING** | 997 | **정상 - 비동기 작업 진행 중** | ✅ 정상 케이스 |
| WSAECONNRESET | 10054 | 원격에서 연결 강제 종료 | ✅ 연결 종료 처리 |
| WSAECONNABORTED | 10053 | 로컬에서 연결 중단 | ✅ 연결 종료 처리 |
| WSAENOBUFS | 10055 | 송신 버퍼 부족 | ⚠️ 일시적, 재시도 |
| WSAEMSGSIZE | 10040 | 메시지가 너무 큼 | ❌ 데이터 분할 필요 |
| WSAENOTCONN | 10057 | 소켓이 연결되지 않음 | ❌ 연결 상태 검증 필요 |

#### 에러 핸들링 예시

```cpp
int result = WSASend(socket, &wsaBuf, 1, nullptr, 0, &overlapped, nullptr);
if (result == SOCKET_ERROR) {
    DWORD error = WSAGetLastError();

    if (error == WSA_IO_PENDING) {
        // 정상 - IOCP 완료 대기
        return SUCCESS;
    }

    switch (error) {
        case WSAECONNRESET:
        case WSAECONNABORTED:
            // 연결 종료
            _logger->Info("Client disconnected during send: {}", error);
            CloseClient(client);
            break;

        case WSAENOBUFS:
            // 송신 버퍼 부족 - 재시도 또는 속도 조절
            _logger->Warn("Send buffer full, throttling");
            break;

        case WSAEMSGSIZE:
            // 메시지 크기 초과
            _logger->Error("Message too large for protocol");
            break;

        default:
            _logger->Error("WSASend failed: {}", error);
            CloseClient(client);
            break;
    }
}
```

---

### WSAIoctl

#### 함수 시그니처
```cpp
int WSAIoctl(
    SOCKET s,                                    // 소켓 핸들
    DWORD dwIoControlCode,                       // 제어 코드 (SIO_*)
    LPVOID lpvInBuffer,                          // 입력 버퍼
    DWORD cbInBuffer,                            // 입력 버퍼 크기
    LPVOID lpvOutBuffer,                         // 출력 버퍼
    DWORD cbOutBuffer,                           // 출력 버퍼 크기
    LPDWORD lpcbBytesReturned,                   // 반환된 바이트 수
    LPWSAOVERLAPPED lpOverlapped,                // Overlapped 구조체
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine  // 완료 루틴
);
```

#### 역할
- 소켓의 동작 모드 제어 및 설정
- **확장 함수 포인터 획득** (AcceptEx, ConnectEx 등)
- Keep-alive, Non-blocking 모드, 네트워크 정보 조회 등 다양한 기능

#### 동작 특성
- **Both**: Overlapped 파라미터에 따라 동기/비동기 선택 가능
- **Both**: 제어 코드와 Overlapped 사용 여부에 따라 블로킹/논블로킹

#### 주요 제어 코드

| 제어 코드 | 용도 |
|----------|------|
| SIO_GET_EXTENSION_FUNCTION_POINTER | AcceptEx 등 확장 함수 포인터 획득 |
| FIONBIO | Non-blocking 모드 설정 |
| FIONREAD | 읽을 수 있는 데이터 크기 조회 |
| SIO_KEEPALIVE_VALS | TCP Keep-alive 타임아웃 설정 |
| SIO_ROUTING_INTERFACE_QUERY | 특정 원격 주소의 로컬 인터페이스 조회 |

#### 주요 에러 코드

| 에러 코드 | 값 | 발생 조건 | 복구 가능 |
|----------|-----|----------|-----------|
| **WSA_IO_PENDING** | 997 | **Overlapped 비동기 작업 진행 중** | ✅ 정상 케이스 |
| WSAEFAULT | 10014 | 버퍼 크기 부족 또는 잘못된 포인터 | ⚠️ 버퍼 크기 조정 후 재시도 |
| WSAEINVAL | 10022 | 잘못된 제어 코드 또는 소켓 상태 | ❌ 파라미터 검증 필요 |
| WSAEOPNOTSUPP | 10045 | 지원하지 않는 IOCTL | ⚠️ Fallback 로직 필요 |
| WSAENOTSOCK | 10038 | 유효하지 않은 소켓 | ❌ 소켓 재생성 필요 |

#### 에러 핸들링 예시 (AcceptEx 함수 포인터 획득)

```cpp
GUID guidAcceptEx = WSAID_ACCEPTEX;
LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
DWORD bytes = 0;

int result = WSAIoctl(listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                      &guidAcceptEx, sizeof(guidAcceptEx),
                      &lpfnAcceptEx, sizeof(lpfnAcceptEx),
                      &bytes, nullptr, nullptr);

if (result == SOCKET_ERROR) {
    DWORD error = WSAGetLastError();
    switch (error) {
        case WSAEFAULT:
            _logger->Error("Buffer size error loading AcceptEx");
            break;
        case WSAENOTSOCK:
            _logger->Error("Invalid socket handle");
            break;
        case WSAEINVAL:
            _logger->Error("Invalid GUID or socket not in listening state");
            break;
        case WSAEOPNOTSUPP:
            _logger->Error("AcceptEx not supported by service provider");
            break;
    }
}

// NULL 체크
if (lpfnAcceptEx == nullptr) {
    _logger->Error("AcceptEx function pointer is NULL");
}
```

---

## IOCP 함수 상세

### CreateIoCompletionPort

#### 함수 시그니처
```cpp
HANDLE CreateIoCompletionPort(
    HANDLE FileHandle,                  // 연결할 핸들 (소켓 등) 또는 INVALID_HANDLE_VALUE
    HANDLE ExistingCompletionPort,      // 기존 IOCP 핸들 또는 NULL
    ULONG_PTR CompletionKey,            // 완료 키 (Client 포인터 등)
    DWORD NumberOfConcurrentThreads     // 동시 실행 스레드 수 (0 = CPU 코어 수)
);
```

#### 역할
- **IOCP 생성**: `FileHandle == INVALID_HANDLE_VALUE` 시 새 IOCP 생성
- **소켓 연결**: `FileHandle`이 유효한 소켓 핸들이면 기존 IOCP에 연결

#### 동작 특성
- **Synchronous**: IOCP 생성/연결이 즉시 완료
- **Non-blocking**: 호출 즉시 반환 (핸들 또는 NULL)

#### 주요 에러 코드

| 에러 코드 | 값 | 발생 조건 | 복구 가능 |
|----------|-----|----------|-----------|
| ERROR_INVALID_PARAMETER | 87 | Overlapped I/O 미지원 핸들 또는 이미 다른 IOCP에 연결됨 | ❌ 소켓 재생성 필요 |
| ERROR_NOT_ENOUGH_MEMORY | 8 | 시스템 메모리 부족 | ❌ 시스템 리소스 문제 |
| ERROR_INVALID_HANDLE | 6 | 유효하지 않은 핸들 | ❌ 핸들 검증 필요 |

#### 에러 핸들링 예시

```cpp
// IOCP 생성
HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
if (iocp == NULL) {
    DWORD error = GetLastError();
    switch (error) {
        case ERROR_NOT_ENOUGH_MEMORY:
            _logger->Critical("System memory exhausted, cannot create IOCP");
            break;
        default:
            _logger->Critical("CreateIoCompletionPort failed: {}", error);
            break;
    }
}

// 소켓을 IOCP에 연결
HANDLE result = CreateIoCompletionPort((HANDLE)socket, iocp,
                                       (ULONG_PTR)clientPtr, 0);
if (result == NULL || result != iocp) {
    DWORD error = GetLastError();
    switch (error) {
        case ERROR_INVALID_PARAMETER:
            _logger->Error("Socket already associated or not created with WSA_FLAG_OVERLAPPED");
            break;
        case ERROR_INVALID_HANDLE:
            _logger->Error("Invalid socket handle");
            break;
    }
}
```

---

### GetQueuedCompletionStatus

#### 함수 시그니처
```cpp
BOOL GetQueuedCompletionStatus(
    HANDLE CompletionPort,               // IOCP 핸들
    LPDWORD lpNumberOfBytesTransferred,  // 전송된 바이트 수
    PULONG_PTR lpCompletionKey,          // 완료 키 (Client 포인터)
    LPOVERLAPPED *lpOverlapped,          // Overlapped 구조체 포인터
    DWORD dwMilliseconds                 // 타임아웃 (INFINITE = 무한 대기)
);
```

#### 역할
- IOCP 큐에서 **단일 완료 패킷** 가져오기
- 완료된 비동기 I/O 작업 정보 반환
- **워커 스레드가 이 함수에서 블로킹**되어 I/O 완료 대기

#### 동작 특성
- **Synchronous**: 함수 자체는 동기 (완료 패킷 대기)
- **Blocking**: 타임아웃까지 대기 (INFINITE 사용 시 무한 대기)

#### 주요 에러 코드

| 에러 코드 | 값 | 의미 | 복구 가능 |
|----------|-----|------|-----------|
| ERROR_ABANDONED_WAIT_0 | 735 | IOCP 핸들이 닫힘 (정상 종료 시나리오) | ✅ 워커 스레드 종료 |
| WAIT_TIMEOUT | 258 | 타임아웃 발생 (dwMilliseconds 내 완료 패킷 없음) | ✅ 재시도 또는 종료 |
| ERROR_NETNAME_DELETED | 64 | 원격 연결 종료 (overlapped != NULL) | ✅ 클라이언트 정리 |
| ERROR_OPERATION_ABORTED | 995 | I/O 취소됨 (CancelIoEx) | ✅ 취소 처리 |
| ERROR_CONNECTION_ABORTED | 1236 | 로컬에서 연결 중단 | ✅ 클라이언트 정리 |

#### 에러 핸들링 예시

```cpp
DWORD bytesTransferred = 0;
ULONG_PTR completionKey = 0;
LPOVERLAPPED overlapped = nullptr;

BOOL ok = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey,
                                    &overlapped, INFINITE);

// Shutdown 신호 체크
if (completionKey == 0 && overlapped == nullptr) {
    _logger->Info("Shutdown signal received");
    return;
}

if (!ok) {
    DWORD error = GetLastError();

    // IOCP 함수 자체 실패 (overlapped == NULL)
    if (overlapped == nullptr) {
        switch (error) {
            case ERROR_ABANDONED_WAIT_0:
                _logger->Info("IOCP closed, worker exiting");
                return;
            case WAIT_TIMEOUT:
                continue;  // 재시도
            default:
                _logger->Error("GQCS failed: {}", error);
                return;
        }
    }

    // I/O 작업 실패 (overlapped != NULL)
    switch (error) {
        case ERROR_NETNAME_DELETED:
        case ERROR_CONNECTION_ABORTED:
        case WSAECONNRESET:
            // 연결 종료
            _logger->Info("Client disconnected: {}", error);
            CloseClient((Client*)completionKey);
            break;
        case ERROR_OPERATION_ABORTED:
            // I/O 취소
            _logger->Debug("I/O cancelled");
            break;
    }
}

// 정상 완료 (ok == TRUE)
if (bytesTransferred == 0) {
    // 0바이트 수신 = Graceful shutdown
    _logger->Info("Client graceful disconnect");
    CloseClient((Client*)completionKey);
}
```

---

### GetQueuedCompletionStatusEx

#### 함수 시그니처
```cpp
BOOL GetQueuedCompletionStatusEx(
    HANDLE CompletionPort,                  // IOCP 핸들
    LPOVERLAPPED_ENTRY lpCompletionPortEntries,  // 완료 패킷 배열
    ULONG ulCount,                          // 배열 크기
    PULONG ulNumEntriesRemoved,             // 실제 가져온 패킷 수
    DWORD dwMilliseconds,                   // 타임아웃
    BOOL fAlertable                         // APC 지원 (보통 FALSE)
);
```

#### 역할
- IOCP 큐에서 **여러 완료 패킷을 일괄(batch) 처리**
- Windows Vista 이상에서 사용 가능
- **고성능 서버에서 GetQueuedCompletionStatus보다 20-40% 성능 향상**

#### 동작 특성
- **Synchronous**: 함수 자체는 동기 (완료 패킷 대기)
- **Blocking**: 타임아웃까지 대기

#### GetQueuedCompletionStatus와의 차이점

| 특성 | GQCS | GQCSEx |
|------|------|--------|
| 배치 처리 | ❌ 단일 패킷 | ✅ 최대 ulCount개 패킷 |
| 커널 전환 | 패킷당 1회 | 배치당 1회 (최대 97.5% 감소) |
| 반환 구조체 | 개별 파라미터 | OVERLAPPED_ENTRY 배열 |
| 성능 (고처리량) | 낮음 | 20-40% 향상 |

#### OVERLAPPED_ENTRY 구조체

```cpp
typedef struct _OVERLAPPED_ENTRY {
    ULONG_PTR lpCompletionKey;              // 완료 키
    LPOVERLAPPED lpOverlapped;              // Overlapped 포인터
    ULONG_PTR Internal;                     // 내부 사용 (NTSTATUS)
    DWORD dwNumberOfBytesTransferred;       // 전송 바이트 수
} OVERLAPPED_ENTRY;
```

#### 에러 핸들링 예시

```cpp
constexpr ULONG BATCH_SIZE = 64;
OVERLAPPED_ENTRY entries[BATCH_SIZE];
ULONG removed = 0;

BOOL ok = GetQueuedCompletionStatusEx(iocp, entries, BATCH_SIZE, &removed,
                                      INFINITE, FALSE);
if (!ok) {
    DWORD error = GetLastError();
    switch (error) {
        case ERROR_ABANDONED_WAIT_0:
            _logger->Info("IOCP closed, worker exiting");
            return;
        case WAIT_TIMEOUT:
            continue;
    }
}

// 배치 처리
for (ULONG i = 0; i < removed; ++i) {
    auto& entry = entries[i];

    // Shutdown 신호 체크
    if (entry.lpCompletionKey == 0 && entry.lpOverlapped == nullptr) {
        return;
    }

    // I/O 성공 여부 확인
    bool ioSuccess = (entry.lpOverlapped->Internal == 0);
    DWORD errorCode = ioSuccess ? 0 :
        RtlNtStatusToDosError(entry.lpOverlapped->Internal);

    if (!ioSuccess) {
        HandleIoError((Client*)entry.lpCompletionKey, errorCode);
        continue;
    }

    // 정상 처리
    ProcessCompletion(entry);
}
```

---

### PostQueuedCompletionStatus

#### 함수 시그니처
```cpp
BOOL PostQueuedCompletionStatus(
    HANDLE       CompletionPort,              // IOCP 핸들
    DWORD        dwNumberOfBytesTransferred,  // 완료 패킷에 포함될 바이트 수
    ULONG_PTR    dwCompletionKey,             // 완료 키
    LPOVERLAPPED lpOverlapped                 // Overlapped 구조체 포인터
);
```

#### 역할
- IOCP 큐에 **완료 패킷을 수동으로 삽입**하는 함수
- 실제 I/O 작업 없이도 워커 스레드에게 사용자 정의 메시지 전달
- **Shutdown 신호 전송**에 가장 많이 사용됨
- 스레드 간 통신 및 제어 신호 전송에 활용

#### 동작 특성
- **Synchronous**: 호출 즉시 큐에 패킷을 삽입하고 반환
- **Non-blocking**: 큐에 패킷을 추가하는 작업만 수행하고 즉시 반환

#### 주요 사용 사례

**1. Shutdown 신호 전송 (가장 일반적)**
```cpp
// IoCompletionPort::Shutdown()에서
for (auto& worker : _workerThreads) {
    worker.request_stop();
    PostQueuedCompletionStatus(_handle, 0, 0, nullptr);  // Shutdown 신호
}

// WorkerLoop에서 수신
if (completionKey == 0 && overlapped == nullptr) {
    break;  // 종료
}
```

**2. 커스텀 메시지 전송**
```cpp
// 특정 클라이언트에게 우선순위 작업 할당
PostQueuedCompletionStatus(iocp, 0, (ULONG_PTR)client, customOverlapped);
```

#### 주요 에러 코드

| 에러 코드 | 값 | 발생 조건 | 복구 가능 |
|----------|-----|----------|-----------|
| ERROR_INVALID_HANDLE | 6 | IOCP 핸들이 유효하지 않음 | ❌ 프로그래밍 오류 |
| ERROR_NOT_ENOUGH_MEMORY | 8 | 시스템 메모리 부족 | ⚠️ 일시적, 재시도 가능 |
| ERROR_INVALID_PARAMETER | 87 | 잘못된 파라미터 (드물음) | ❌ 파라미터 검증 필요 |

#### 에러 핸들링 예시

```cpp
BOOL result = PostQueuedCompletionStatus(_handle, 0, 0, nullptr);
if (result == FALSE) {
    DWORD error = GetLastError();

    switch (error) {
        case ERROR_INVALID_HANDLE:
            _logger->Error("PostCompletion failed: Invalid IOCP handle");
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
            _logger->Error("PostCompletion failed: Insufficient memory");
            break;
        default:
            _logger->Error("PostCompletion failed: Error {}", error);
            break;
    }
}
```

#### GetQueuedCompletionStatus와의 관계

```
Post 측:
PostQueuedCompletionStatus(iocp, 1024, 0xABCD, ptr);
                           ↓
                     IOCP 큐에 삽입
                           ↓
Get 측:
GetQueuedCompletionStatus(..., &bytes, &key, &overlapped, ...);
                           ↓
                     bytes = 1024  (동일)
                     key   = 0xABCD (동일)
                     overlapped = ptr (동일)
```

#### 현재 코드베이스 사용 분석

**IoCompletionPort.cpp:49** - Shutdown에서 사용
```cpp
// ⚠️ 개선 필요: 에러 체크 없음
PostQueuedCompletionStatus(_handle, 0, 0, nullptr);
```

**IoCompletionPort.cpp:77** - PostCompletion에서 사용
```cpp
// ✅ 에러 체크 있음 (Result 타입 반환)
BOOL result = PostQueuedCompletionStatus(_handle, bytes,
                                         reinterpret_cast<ULONG_PTR>(key),
                                         overlapped);
if (result == FALSE) {
    return Res::Err(err::EIocpError::IocpInternalError);
}
```

**권장 개선:**
- Shutdown에도 에러 로깅 추가
- GetLastError() 호출하여 상세 에러 정보 기록
- 핸들 유효성 사전 검증

---

### CancelIoEx

#### 함수 시그니처
```cpp
BOOL CancelIoEx(
    HANDLE hFile,                       // 소켓 핸들
    LPOVERLAPPED lpOverlapped           // 취소할 Overlapped (NULL = 모두 취소)
);
```

#### 역할
- 보류 중인 비동기 I/O 작업을 취소
- **크로스 스레드 취소 가능** (다른 스레드에서 발생시킨 I/O도 취소)
- lpOverlapped로 특정 I/O만 선택적 취소 가능

#### 동작 특성
- **Synchronous**: 함수 자체는 동기 (취소 요청만 수행)
- **Non-blocking**: 즉시 반환 (실제 I/O 완료는 비동기로 IOCP 통지)

#### CancelIo와의 차이점

| 특성 | CancelIo | CancelIoEx |
|------|----------|------------|
| 스레드 범위 | 호출 스레드만 | 모든 스레드 |
| 세밀한 제어 | 핸들의 모든 I/O | 특정 Overlapped 지정 가능 |
| 크로스 스레드 | 불가능 | 가능 |

#### 주요 에러 코드

| 에러 코드 | 값 | 의미 | 복구 가능 |
|----------|-----|------|-----------|
| ERROR_NOT_FOUND | 1168 | 취소할 I/O가 없음 (이미 완료됨) | ✅ 성공으로 처리 (Race condition) |
| ERROR_INVALID_HANDLE | 6 | 유효하지 않은 핸들 | ❌ 핸들 검증 필요 |

#### 완료 패킷의 에러 코드

취소 성공 시 IOCP에서 다음 에러를 받음:
- **ERROR_OPERATION_ABORTED (995)**: I/O가 성공적으로 취소됨

#### 에러 핸들링 예시

```cpp
BOOL result = CancelIoEx((HANDLE)socket, &overlapped);
if (!result) {
    DWORD error = GetLastError();

    if (error == ERROR_NOT_FOUND) {
        // I/O가 이미 완료됨 - 정상 (Race condition)
        _logger->Debug("I/O already completed before cancel");
        return SUCCESS;  // 성공으로 처리
    }

    switch (error) {
        case ERROR_INVALID_HANDLE:
            _logger->Error("Invalid socket handle for CancelIoEx");
            break;
        default:
            _logger->Error("CancelIoEx failed: {}", error);
            break;
    }
    return ERROR;
}

// 취소 요청 성공 - IOCP에서 ERROR_OPERATION_ABORTED 완료 패킷을 받을 것
// ⚠️ 주의: 완료 패킷을 받을 때까지 Overlapped 구조체를 해제하면 안 됨!
```

#### IOCP 환경 주의사항

```cpp
// ❌ 위험한 코드
CancelIoEx((HANDLE)socket, &overlapped);
delete pOverlapped;  // Race! 아직 커널이 사용 중일 수 있음

// ✅ 안전한 코드
CancelIoEx((HANDLE)socket, &overlapped);
// IOCP worker에서 ERROR_OPERATION_ABORTED 수신 후 삭제

// IOCP Worker Thread
void HandleCompletion(IOContext* ioCtx, DWORD error) {
    if (error == ERROR_OPERATION_ABORTED) {
        // 취소됨 - 이제 안전하게 정리 가능
        delete ioCtx;
    }
}
```

---

## 함수 비교표

### 에러 반환 방식

| 함수 | 성공 반환값 | 실패 반환값 | 에러 코드 획득 |
|------|-------------|-------------|----------------|
| WSASocket | 유효한 SOCKET | INVALID_SOCKET | WSAGetLastError() |
| bind | 0 | SOCKET_ERROR (-1) | WSAGetLastError() |
| listen | 0 | SOCKET_ERROR (-1) | WSAGetLastError() |
| setsockopt | 0 | SOCKET_ERROR (-1) | WSAGetLastError() |
| AcceptEx | TRUE 또는 FALSE+IO_PENDING | FALSE (에러) | WSAGetLastError() |
| WSARecv | 0 또는 SOCKET_ERROR+IO_PENDING | SOCKET_ERROR (에러) | WSAGetLastError() |
| WSASend | 0 또는 SOCKET_ERROR+IO_PENDING | SOCKET_ERROR (에러) | WSAGetLastError() |
| WSAIoctl | 0 또는 SOCKET_ERROR+IO_PENDING | SOCKET_ERROR (에러) | WSAGetLastError() |
| CreateIoCompletionPort | 유효한 HANDLE | NULL | GetLastError() |
| GetQueuedCompletionStatus | TRUE | FALSE | GetLastError() |
| GetQueuedCompletionStatusEx | TRUE | FALSE | GetLastError() |
| PostQueuedCompletionStatus | TRUE (Non-zero) | FALSE (Zero) | GetLastError() |
| CancelIoEx | TRUE | FALSE | GetLastError() |

### 비동기 작업 정상 케이스

| 함수 | 정상 비동기 시작 조건 |
|------|----------------------|
| AcceptEx | `FALSE` 반환 + `WSAGetLastError() == ERROR_IO_PENDING` |
| WSARecv | `SOCKET_ERROR` 반환 + `WSAGetLastError() == WSA_IO_PENDING` |
| WSASend | `SOCKET_ERROR` 반환 + `WSAGetLastError() == WSA_IO_PENDING` |
| WSAIoctl (Overlapped) | `SOCKET_ERROR` 반환 + `WSAGetLastError() == WSA_IO_PENDING` |

### 복구 가능 에러 유형

| 복구 가능성 | 에러 예시 | 대응 방법 |
|-------------|----------|----------|
| ✅ **복구 가능** | ERROR_NOT_FOUND (CancelIoEx), WSAECONNRESET (AcceptEx), WSAENOBUFS | 재시도 또는 무시 |
| ⚠️ **제한적 복구** | WSAEADDRINUSE, WSAEWOULDBLOCK | 조건 변경 후 재시도 |
| ❌ **복구 불가능** | WSAENOTSOCK, WSAEINVAL, ERROR_INVALID_HANDLE | 소켓/핸들 재생성 또는 프로그램 종료 |

---

## 실전 패턴

### 패턴 1: IOCP 서버 초기화

```cpp
// 1. Winsock 초기화
WSADATA wsaData;
if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    // ERROR
}

// 2. 소켓 생성 (Overlapped I/O 플래그 필수)
SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                 nullptr, 0, WSA_FLAG_OVERLAPPED);
if (listenSocket == INVALID_SOCKET) {
    // ERROR
}

// 3. bind + listen
SOCKADDR_IN addr = {};
addr.sin_family = AF_INET;
addr.sin_port = htons(8080);
addr.sin_addr.s_addr = INADDR_ANY;

bind(listenSocket, (SOCKADDR*)&addr, sizeof(addr));
listen(listenSocket, SOMAXCONN);

// 4. IOCP 생성
HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);

// 5. Listen 소켓을 IOCP에 연결
CreateIoCompletionPort((HANDLE)listenSocket, iocp, 0, 0);

// 6. AcceptEx 함수 포인터 획득
LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
GUID guidAcceptEx = WSAID_ACCEPTEX;
DWORD bytes = 0;
WSAIoctl(listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
         &guidAcceptEx, sizeof(guidAcceptEx),
         &lpfnAcceptEx, sizeof(lpfnAcceptEx),
         &bytes, nullptr, nullptr);

// 7. 워커 스레드 생성
for (int i = 0; i < workerCount; ++i) {
    std::thread worker(WorkerThreadFunc, iocp);
    worker.detach();
}

// 8. AcceptEx 호출 (여러 개 미리 등록)
for (int i = 0; i < backlog; ++i) {
    PostAccept(listenSocket, iocp, lpfnAcceptEx);
}
```

### 패턴 2: AcceptEx 비동기 호출

```cpp
void PostAccept(SOCKET listenSocket, HANDLE iocp, LPFN_ACCEPTEX lpfnAcceptEx) {
    // 1. Accept 소켓 생성
    SOCKET acceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                    nullptr, 0, WSA_FLAG_OVERLAPPED);

    // 2. Overlapped 구조체 준비
    auto* overlapped = new OverlappedExt{};
    overlapped->ioType = IoType::Accept;
    overlapped->clientSocket = acceptSocket;

    // 3. AcceptEx 호출
    DWORD bytesReceived = 0;
    constexpr DWORD addrLen = sizeof(SOCKADDR_IN) + 16;

    BOOL result = lpfnAcceptEx(
        listenSocket,
        acceptSocket,
        overlapped->buffer,
        0,                      // 초기 데이터 수신 안 함
        addrLen,
        addrLen,
        &bytesReceived,
        (LPOVERLAPPED)overlapped
    );

    if (result == FALSE) {
        DWORD error = WSAGetLastError();
        if (error != ERROR_IO_PENDING) {
            // 에러 처리
            closesocket(acceptSocket);
            delete overlapped;
        }
        // ERROR_IO_PENDING은 정상 (IOCP 완료 대기)
    }
}
```

### 패턴 3: IOCP 워커 스레드

```cpp
void WorkerThreadFunc(HANDLE iocp) {
    while (true) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED overlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(iocp, &bytesTransferred,
                                            &completionKey, &overlapped,
                                            INFINITE);

        // Shutdown 신호 체크
        if (completionKey == 0 && overlapped == nullptr) {
            break;
        }

        auto* ext = (OverlappedExt*)overlapped;
        auto* client = (Client*)completionKey;

        // 에러 처리
        if (!ok || bytesTransferred == 0) {
            HandleError(client, ext, GetLastError());
            continue;
        }

        // I/O 타입별 처리
        switch (ext->ioType) {
            case IoType::Accept:
                OnAccept(ext);
                break;
            case IoType::Recv:
                OnRecv(client, ext, bytesTransferred);
                break;
            case IoType::Send:
                OnSend(client, ext, bytesTransferred);
                break;
        }
    }
}
```

### 패턴 4: Accept 완료 처리

```cpp
void OnAccept(OverlappedExt* overlapped) {
    // 1. SO_UPDATE_ACCEPT_CONTEXT 설정 (필수!)
    setsockopt(overlapped->clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
               (char*)&listenSocket, sizeof(listenSocket));

    // 2. Client 객체 생성
    auto client = std::make_shared<Client>();
    client->socket = overlapped->clientSocket;

    // 3. 클라이언트 소켓을 IOCP에 연결
    CreateIoCompletionPort((HANDLE)client->socket, iocp,
                           (ULONG_PTR)client.get(), 0);

    // 4. 첫 수신 시작
    client->PostRecv();

    // 5. Accept Overlapped 정리 및 재등록
    delete overlapped;
    PostAccept(listenSocket, iocp, lpfnAcceptEx);
}
```

### 패턴 5: Recv/Send 비동기 처리

```cpp
class Client {
    SOCKET socket;
    OverlappedExt recvOverlapped;
    OverlappedExt sendOverlapped;

    void PostRecv() {
        ZeroMemory(&recvOverlapped.overlapped, sizeof(WSAOVERLAPPED));

        recvOverlapped.wsaBuffer.buf = recvOverlapped.buffer;
        recvOverlapped.wsaBuffer.len = 4096;
        recvOverlapped.ioType = IoType::Recv;

        DWORD flags = 0;
        int result = WSARecv(socket, &recvOverlapped.wsaBuffer, 1, nullptr,
                             &flags, &recvOverlapped.overlapped, nullptr);

        if (result == SOCKET_ERROR) {
            DWORD error = WSAGetLastError();
            if (error != WSA_IO_PENDING) {
                // 에러 처리
            }
        }
    }

    void PostSend(const char* data, size_t len) {
        ZeroMemory(&sendOverlapped.overlapped, sizeof(WSAOVERLAPPED));

        memcpy(sendOverlapped.buffer, data, len);
        sendOverlapped.wsaBuffer.buf = sendOverlapped.buffer;
        sendOverlapped.wsaBuffer.len = len;
        sendOverlapped.ioType = IoType::Send;

        int result = WSASend(socket, &sendOverlapped.wsaBuffer, 1, nullptr,
                             0, &sendOverlapped.overlapped, nullptr);

        if (result == SOCKET_ERROR) {
            DWORD error = WSAGetLastError();
            if (error != WSA_IO_PENDING) {
                // 에러 처리
            }
        }
    }
};
```

### 패턴 6: Graceful Shutdown

```cpp
void ShutdownServer(HANDLE iocp, std::vector<std::thread>& workers) {
    // 1. 모든 워커 스레드에 종료 신호 전송
    for (size_t i = 0; i < workers.size(); ++i) {
        PostQueuedCompletionStatus(iocp, 0, 0, nullptr);
    }

    // 2. 워커 스레드 종료 대기
    for (auto& worker : workers) {
        worker.join();
    }

    // 3. IOCP 핸들 닫기
    CloseHandle(iocp);

    // 4. Winsock 정리
    WSACleanup();
}
```

### 패턴 7: 에러 처리 통합

```cpp
enum class ErrorAction {
    Continue,           // 정상 처리 계속
    CloseConnection,    // 해당 연결만 종료
    Shutdown            // 전체 서버 종료
};

ErrorAction HandleIoError(Client* client, DWORD error) {
    switch (error) {
        // 정상적인 연결 종료
        case ERROR_NETNAME_DELETED:
        case ERROR_CONNECTION_ABORTED:
        case WSAECONNRESET:
        case 0:  // bytesTransferred == 0
            _logger->Info("Client disconnected gracefully");
            return ErrorAction::CloseConnection;

        // I/O 취소
        case ERROR_OPERATION_ABORTED:
            _logger->Debug("I/O operation cancelled");
            return ErrorAction::CloseConnection;

        // 일시적 에러
        case WSAENOBUFS:
            _logger->Warn("System buffer shortage");
            return ErrorAction::Continue;

        // 치명적 에러
        case ERROR_INVALID_HANDLE:
            _logger->Critical("Invalid socket handle");
            return ErrorAction::Shutdown;

        default:
            _logger->Error("Unknown I/O error: {}", error);
            return ErrorAction::CloseConnection;
    }
}
```

---

## 참고 자료

### Microsoft 공식 문서

- [Winsock Reference - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/winsock/winsock-reference)
- [Windows Sockets Error Codes](https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2)
- [I/O Completion Ports](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
- [AcceptEx function](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex)

### 추천 학습 자료

- **Windows via C/C++** (Jeffrey Richter) - IOCP 상세 설명
- **Network Programming for Microsoft Windows** - Winsock 완벽 가이드
- [The Winsock Programmer's FAQ](http://tangentsoft.net/wskfaq/) - 실전 팁

### 코드 예제

- [Windows-classic-samples/IOCP](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/netds/winsock/iocp)
- [libuv](https://github.com/libuv/libuv) - IOCP 기반 크로스 플랫폼 라이브러리

---

**문서 버전**: 1.0
**최종 수정**: 2026-01-20
**작성자**: Claude Code Analysis Team

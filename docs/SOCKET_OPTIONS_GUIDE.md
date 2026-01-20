# Windows Winsock 소켓 옵션 완벽 가이드

**작성일**: 2026-01-20
**목적**: IOCP 기반 게임 서버 개발을 위한 소켓 옵션 레퍼런스
**대상**: 고성능 MMORPG 서버 개발자

---

## 📚 목차

1. [개요](#개요)
2. [SOL_SOCKET 레벨 옵션](#sol_socket-레벨-옵션)
3. [IPPROTO_TCP 레벨 옵션](#ipproto_tcp-레벨-옵션)
4. [IPPROTO_IP 레벨 옵션](#ipproto_ip-레벨-옵션)
5. [IOCP 서버 Best Practices](#iocp-서버-best-practices)
6. [현재 코드베이스 분석](#현재-코드베이스-분석)
7. [참고 자료](#참고-자료)

---

## 개요

소켓 옵션은 `setsockopt()`와 `getsockopt()` 함수를 통해 소켓의 동작을 세밀하게 제어할 수 있는 메커니즘입니다. 이 문서는 IOCP 기반 게임 서버 개발에 필요한 핵심 소켓 옵션들을 다룹니다.

### 함수 시그니처

```cpp
int setsockopt(
    SOCKET       s,         // 소켓 핸들
    int          level,     // 옵션 레벨 (SOL_SOCKET, IPPROTO_TCP, IPPROTO_IP)
    int          optname,   // 옵션 이름 (SO_REUSEADDR, TCP_NODELAY 등)
    const char   *optval,   // 옵션 값 포인터
    int          optlen     // 옵션 값 크기
);

int getsockopt(
    SOCKET       s,
    int          level,
    int          optname,
    char         *optval,   // 출력: 현재 옵션 값
    int          *optlen    // 입출력: 버퍼 크기
);
```

### 중요도 표시

| 기호 | 의미 |
|------|------|
| ⭐⭐⭐⭐⭐ | 필수 - 반드시 설정해야 함 |
| ⭐⭐⭐⭐ | 강력 권장 - 대부분의 경우 필요 |
| ⭐⭐⭐ | 권장 - 상황에 따라 유용 |
| ⭐⭐ | 선택적 - 특수 케이스만 |
| ⭐ | 거의 불필요 - 특수 상황만 |
| ⛔ | 사용 금지 - 보안/안정성 위험 |

---

## SOL_SOCKET 레벨 옵션

### SO_EXCLUSIVEADDRUSE ⭐⭐⭐⭐⭐

**설명**: 다른 소켓이 동일한 주소와 포트에 강제로 바인딩되는 것을 방지합니다.

**타입**: `BOOL`
**기본값**: `FALSE`
**지원**: Windows NT 4.0 SP4 이상

#### Use Cases
- ✅ **모든 서버 애플리케이션에서 필수 보안 설정**
- ✅ 포트 하이재킹 공격 방지
- ✅ 다른 애플리케이션이 요청한 포트를 이미 사용 중인지 확인

#### 주의사항
- SO_REUSEADDR과 함께 사용할 수 없음
- 멀티캐스트 소켓에는 사용하지 말 것

#### IOCP 서버 권장사항
- **필수**: 모든 서버는 반드시 설정
- **타이밍**: `bind()` 전
- **성능 영향**: 없음

```cpp
BOOL exclusive = TRUE;
if (setsockopt(listenSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               (char*)&exclusive, sizeof(BOOL)) == SOCKET_ERROR) {
    _logger->Error("Failed to set SO_EXCLUSIVEADDRUSE: {}", WSAGetLastError());
    return ERROR;
}
```

---

### SO_REUSEADDR ⛔

**설명**: 이미 사용 중인 주소와 포트에 소켓을 강제로 바인딩할 수 있게 허용합니다.

**타입**: `BOOL`
**기본값**: `FALSE`

#### ⚠️ Windows에서의 보안 위험

Windows에서 SO_REUSEADDR은 다른 OS(Linux/Unix)와 달리 **활성 listening 소켓의 포트도 탈취 가능**합니다.

**공격 시나리오**:
```
1. 정상 서버가 port 8080에서 리슨 중
2. 악의적 프로그램이 SO_REUSEADDR 설정 후 port 8080에 bind
3. 새로운 클라이언트 연결이 악의적 프로그램으로 라우팅됨
4. 패킷 스니핑, MITM 공격 가능
```

#### IOCP 서버 권장사항
- **절대 사용 금지** ⛔
- **대안**: SO_EXCLUSIVEADDRUSE 사용
- TIME_WAIT 문제는 다른 방법으로 해결 (Graceful shutdown)

---

### SO_KEEPALIVE ⭐⭐⭐

**설명**: TCP 연결에서 keepalive 메시지를 전송하도록 설정합니다.

**타입**: `BOOL`
**기본값**: `FALSE`

#### Windows 기본 동작
- **Keepalive timeout**: 2시간 (첫 keepalive 패킷 전송까지 대기)
- **Keepalive interval**: 1초 (후속 keepalive 패킷 간격)
- **Keepalive count**: 10회 (Vista+), 5회 (XP/2003/2000)

#### Use Cases
- ✅ 유휴 연결의 상태 확인
- ✅ 크래시된 클라이언트 감지 (OS 레벨에서)
- ✅ 중간 방화벽/NAT의 연결 타임아웃 방지

#### 게임 서버 고려사항

**기본 설정의 문제점**:
- 기본 2시간 타임아웃은 게임 서버에 부적합
- 죽은 연결을 2시간 후에야 감지하는 것은 리소스 낭비

**권장 설정**:
```cpp
// 1. SO_KEEPALIVE 활성화
BOOL enable = TRUE;
setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&enable, sizeof(BOOL));

// 2. 커스텀 타임아웃 설정 (WSAIoctl)
struct tcp_keepalive alive;
alive.onoff = TRUE;
alive.keepalivetime = 60000;      // 60초 후 첫 keepalive
alive.keepaliveinterval = 10000;  // 10초 간격으로 재시도
DWORD bytesReturned;
WSAIoctl(socket, SIO_KEEPALIVE_VALS, &alive, sizeof(alive),
         NULL, 0, &bytesReturned, NULL, NULL);
```

**게임 서버 Best Practice**:
- SO_KEEPALIVE는 백업 수단으로 활용
- **애플리케이션 레벨 heartbeat**가 더 효과적:
  - 게임 로직에 통합 가능
  - 더 빠른 감지 (30초 단위 권장)
  - 네트워크 상태 모니터링 가능

#### IOCP 서버 권장사항
- **권장**: 백업 감지 메커니즘으로 활용
- **타이밍**: `accept()` 후
- **성능 영향**: 낮음 (패킷 오버헤드 미미)

---

### SO_LINGER ⭐⭐

**설명**: `closesocket()` 호출 시 소켓 종료 동작을 제어합니다.

**타입**: `struct linger { u_short l_onoff; u_short l_linger; }`
**기본값**: `l_onoff = 0` (graceful close)

#### 세 가지 동작 모드

| l_onoff | l_linger | 동작 | 설명 |
|---------|----------|------|------|
| 0 | 무시 | **Graceful close** (기본) | closesocket() 즉시 반환, 백그라운드에서 대기 중인 데이터 전송 시도 |
| ≠0 | 0 | **Hard/Abortive close** | RST 전송, 대기 중인 데이터 손실, 원격은 WSAECONNRESET 수신 |
| ≠0 | >0 | **Timed graceful close** | l_linger 초 동안 블로킹, 데이터 전송 완료 또는 타임아웃 대기 |

#### Use Cases

**Hard close (l_onoff=1, l_linger=0)**:
- ✅ 즉시 연결 종료 필요 시 (악의적 클라이언트 강제 종료)
- ✅ DoS 공격 대응 시 빠른 리소스 회수
- ⚠️ TIME_WAIT 회피 가능하지만 연결 재설정 문제 발생 가능

**기본값 권장**:
- 대부분의 경우 기본 graceful close가 적합
- 명시적 `shutdown()` + `closesocket()` 호출이 더 안전

#### 주의사항
- ⚠️ **Timed close는 워커 스레드 블로킹** - IOCP 환경에서 절대 사용 금지
- Hard close는 TIME_WAIT 회피하지만 안정성 문제
- **Best Practice**: `shutdown(SD_SEND)` + `closesocket()`이 가장 안전

#### IOCP 서버 권장사항
- **선택적**: Hard close만 특수 케이스에서 사용
- **타이밍**: `closesocket()` 전
- **성능 영향**: 중간 (잘못 사용 시 워커 스레드 블로킹)

```cpp
void Client::Close(bool isFireAndForget) {
    if (isFireAndForget) {
        // Hard close: 악의적 클라이언트용
        linger linger{0, 0};
        linger.l_onoff = 1;
        linger.l_linger = 0;
        setsockopt(socket, SOL_SOCKET, SO_LINGER,
                   (char*)&linger, sizeof(linger));
    } else {
        // Graceful close (권장)
        shutdown(socket, SD_SEND);  // 송신 종료 알림
    }
    closesocket(socket);
}
```

---

### SO_RCVBUF / SO_SNDBUF ⭐

**설명**: TCP/IP 스택의 수신/송신 버퍼 크기를 설정합니다.

**타입**: `int` (바이트 단위)
**기본값**: 구현 의존 (보통 8KB~4MB, 최신 OS는 1-4MB)

#### IOCP 전용 권장사항

**기본값 사용 권장** ✅:
- IOCP 서버에서는 Overlapped I/O를 항상 사용
- 커널이 자동으로 최적 버퍼 크기 조정
- 구식 가이드(Q181611)의 "버퍼 크기 0 설정"은 Win2K 이후 불필요

**특수 케이스**:
- Streaming data with overlapped I/O: SO_SNDBUF를 0으로 설정 고려
- UDP나 non-overlapped I/O: 버퍼 크기 조정이 성능에 영향 가능

#### 주의사항
⚠️ **SO_RCVBUF는 listen socket에 설정해야 함**:
- Accept된 소켓에 설정해도 TCP handshake 후라 무효
- Listen socket에 설정하면 accepted socket에 상속됨

```cpp
// Listen socket에 설정 (accept 전)
int recvBufSize = 65536; // 64KB
setsockopt(listenSocket, SOL_SOCKET, SO_RCVBUF,
           (char*)&recvBufSize, sizeof(int));
```

#### IOCP 서버 권장사항
- **선택적**: 대부분 기본값 사용
- **타이밍**: SO_RCVBUF는 `listen()` 전, SO_SNDBUF는 언제나
- **성능 영향**: IOCP에서는 낮음

---

### SO_UPDATE_ACCEPT_CONTEXT ⭐⭐⭐⭐⭐

**설명**: AcceptEx()로 수락된 소켓이 표준 Winsock 함수를 사용할 수 있도록 컨텍스트를 업데이트합니다.

**타입**: `SOCKET` (listening socket handle)
**기본값**: N/A

#### Use Cases
- ✅ **AcceptEx() 완료 후 반드시 호출**해야 함
- ✅ `getpeername()`, `setsockopt()` 등 표준 함수 사용 가능하게 함

#### 주의사항
- ⚠️ **AcceptEx() 전용** - 표준 `accept()`에서는 불필요
- ⚠️ 설정 실패 시 accepted socket의 많은 함수가 오작동
- Listening socket handle을 인자로 전달

#### IOCP 서버 권장사항
- **필수**: AcceptEx 사용 시 즉시 설정
- **타이밍**: AcceptEx() 완료 직후
- **성능 영향**: 없음 (메타데이터만 업데이트)

```cpp
// 현재 코드베이스 예시 (Acceptor.cpp:161-166)
int result = setsockopt(
    overlapped->clientSocket,         // accepted socket
    SOL_SOCKET,
    SO_UPDATE_ACCEPT_CONTEXT,
    reinterpret_cast<char*>(&_listenSocket),  // listening socket
    sizeof(_listenSocket));

if (result == SOCKET_ERROR) {
    _logger->Error("SO_UPDATE_ACCEPT_CONTEXT failed. error: {}", WSAGetLastError());
    closesocket(overlapped->clientSocket);
    return Res::Err(err::ESocketError::AcceptFailed);
}
```

---

### SO_OOBINLINE ⭐

**설명**: OOB(Out-Of-Band) 데이터를 일반 데이터 스트림에 포함시킵니다.

**타입**: `BOOL`
**기본값**: `FALSE`

#### Use Cases
- Urgent data를 MSG_OOB 플래그 없이 일반 recv()로 수신
- 레거시 프로토콜 호환성

#### 게임 서버 고려사항
- OOB는 TCP urgent pointer 사용 (신뢰성 낮음)
- 게임 서버에서는 **애플리케이션 레벨 우선순위 큐**가 더 효과적
- 현대적 네트워크 설계에서는 거의 사용 안 함

#### IOCP 서버 권장사항
- **거의 불필요**: 특수 레거시 프로토콜만
- **타이밍**: 언제나
- **성능 영향**: 없음

---

## IPPROTO_TCP 레벨 옵션

### TCP_NODELAY ⭐⭐⭐⭐⭐

**설명**: Nagle 알고리즘을 비활성화하여 작은 패킷도 즉시 전송합니다.

**타입**: `BOOL`
**기본값**: `FALSE` (Nagle 알고리즘 활성화됨)

#### Nagle 알고리즘

**동작 방식**:
- 작은 패킷들을 모아서 전송하여 네트워크 효율성 향상
- ACK를 받을 때까지 작은 패킷 버퍼링

**단점**:
- 레이턴시 증가 (데이터가 버퍼링되어 지연)
- Interactive application에 부적합

#### Use Cases
- ✅ **게임 서버 필수**: 레이턴시가 중요한 실시간 애플리케이션
- ✅ Interactive applications (SSH, Telnet)
- ✅ Small message 기반 프로토콜

#### 게임 서버 고려사항

**MMORPG 특성**:
- 60-150바이트의 작은 패킷을 빈번히 전송
- Nagle 활성화 시 **7%의 연결에서 20% 이상 지연** 증가 (연구 결과)

**연구 데이터** (John Nagle의 분석):
- MMORPG 트래픽의 93%가 Nagle에 영향받지 않음
- 하지만 나머지 7%는 심각한 지연 발생
- **TCP_NODELAY = TRUE 강력 권장**

**주의사항**:
- TCP_NODELAY 활성화 시 패킷 수 증가 (대역폭 오버헤드)
- 게임 서버는 application-limited이므로 문제없음
- Large bulk transfer에서는 오히려 비효율적

#### IOCP 서버 권장사항
- **필수**: 모든 게임 서버는 반드시 설정
- **타이밍**: `accept()` 후 즉시
- **성능 영향**: 레이턴시 크게 감소, 패킷 수 증가

```cpp
BOOL nodelay = TRUE;
if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
               (char*)&nodelay, sizeof(BOOL)) == SOCKET_ERROR) {
    _logger->Warn("Failed to set TCP_NODELAY: {}", WSAGetLastError());
}
```

---

### TCP_KEEPALIVE (Windows 10 1709+) ⭐⭐⭐

**설명**: TCP keepalive 첫 패킷 전송까지 대기 시간을 설정합니다.

**타입**: `DWORD` (초 단위)
**기본값**: 7200초 (2시간)
**지원**: Windows 10 version 1709 이상

#### Use Cases
- SO_KEEPALIVE와 함께 사용하여 더 세밀한 제어
- Unix/Linux의 TCP_KEEPIDLE과 동일

#### 주의사항
- 구버전 Windows에서는 SIO_KEEPALIVE_VALS 사용 필요
- SO_KEEPALIVE가 활성화되어 있어야 함

#### IOCP 서버 권장사항
- **권장**: SO_KEEPALIVE 사용 시
- **타이밍**: SO_KEEPALIVE 설정 후
- **성능 영향**: 낮음

```cpp
// Windows 10 1709+ 전용
DWORD keepalive_time = 60; // 60초
setsockopt(socket, IPPROTO_TCP, TCP_KEEPALIVE,
           (char*)&keepalive_time, sizeof(DWORD));
```

---

### TCP_KEEPCNT (Windows 10 1703+) ⭐⭐

**설명**: Keepalive 패킷 재시도 횟수를 설정합니다.

**타입**: `DWORD`
**기본값**: 10회 (Vista+)
**지원**: Windows 10 version 1703 이상

#### IOCP 서버 권장사항
- **선택적**: 기본값 10회가 대부분 적절
- **타이밍**: SO_KEEPALIVE 설정 후
- **성능 영향**: 낮음

---

### TCP_KEEPINTVL (Windows 10 1709+) ⭐⭐

**설명**: Keepalive 재시도 패킷 간격을 설정합니다.

**타입**: `DWORD` (초 단위)
**기본값**: 1초
**지원**: Windows 10 version 1709 이상

#### 주의사항
- 구버전 Windows에서는 SIO_KEEPALIVE_VALS 사용

#### IOCP 서버 권장사항
- **선택적**: 기본값 1초가 적절
- **타이밍**: SO_KEEPALIVE 설정 후
- **성능 영향**: 낮음

---

## IPPROTO_IP 레벨 옵션

### IP_TTL ⭐

**설명**: 발신 IP 데이터그램의 TTL(Time To Live) 필드를 설정합니다.

**타입**: `int`
**기본값**: 64 또는 128 (OS 의존)

#### Use Cases
- 라우팅 루프 방지
- Network diagnostics (traceroute)

#### IOCP 서버 권장사항
- **거의 불필요**: 기본값 사용
- **타이밍**: 언제나
- **성능 영향**: 없음

---

### IP_MULTICAST_TTL ⭐

**설명**: 멀티캐스트 트래픽의 TTL 값을 설정합니다.

**타입**: `int`
**기본값**: 1 (로컬 서브넷만)

#### Use Cases
- 멀티캐스트 범위 제어
  - 1 = 로컬 서브넷
  - 32 이하 = organization
  - 255 = global

#### IOCP 서버 권장사항
- **선택적**: 멀티캐스트 사용 시만
- **타이밍**: sendto() 전
- **성능 영향**: 없음

---

### IP_MULTICAST_LOOP ⭐

**설명**: 로컬 머신이 전송한 멀티캐스트 패킷을 자신도 수신할지 설정합니다.

**타입**: `BOOL`
**기본값**: `TRUE`

#### Use Cases
- 동일 머신의 여러 프로세스 간 멀티캐스트 통신
- 불필요한 경우 FALSE로 설정하여 오버헤드 감소

#### IOCP 서버 권장사항
- **선택적**: 멀티캐스트 사용 시만
- **타이밍**: 언제나
- **성능 영향**: 낮음

---

### IP_ADD_MEMBERSHIP / IP_DROP_MEMBERSHIP ⭐

**설명**: 멀티캐스트 그룹에 가입/탈퇴합니다.

**타입**: `struct ip_mreq`
**기본값**: N/A

```cpp
struct ip_mreq {
    struct in_addr imr_multiaddr;  // 멀티캐스트 그룹 주소
    struct in_addr imr_interface;  // 로컬 인터페이스 주소
};
```

#### Use Cases
- 멀티캐스트 수신을 위한 그룹 가입
- 서버 discovery, broadcasting

#### 주의사항
- 헤더 파일 호환성 중요 (`Ws2tcpip.h` 필요)
- 잘못된 헤더 사용 시 WSAENOPROTOOPT 에러

#### IOCP 서버 권장사항
- **선택적**: 멀티캐스트 사용 시만
- **타이밍**: bind() 후
- **성능 영향**: 없음

---

## IOCP 서버 Best Practices

### 권장 소켓 옵션 설정 조합

#### 1. Listening Socket 설정 (bind 전)

```cpp
SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                nullptr, 0, WSA_FLAG_OVERLAPPED);

// [필수] 보안: 포트 하이재킹 방지
BOOL exclusive = TRUE;
if (setsockopt(listenSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               (char*)&exclusive, sizeof(BOOL)) == SOCKET_ERROR) {
    _logger->Error("Failed to set SO_EXCLUSIVEADDRUSE: {}", WSAGetLastError());
    return ERROR;
}
```

#### 2. Listening Socket 설정 (listen 전, 선택사항)

```cpp
// [선택적] 대규모 서버에서 버퍼 조정 필요 시
// IOCP 환경에서는 대부분 기본값 사용 권장
int recvBufSize = 65536;
setsockopt(listenSocket, SOL_SOCKET, SO_RCVBUF,
           (char*)&recvBufSize, sizeof(int));
```

#### 3. Accepted Socket 설정 (AcceptEx 완료 직후)

```cpp
// [필수] AcceptEx 사용 시 컨텍스트 업데이트
if (setsockopt(acceptedSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
               (char*)&listenSocket, sizeof(listenSocket)) == SOCKET_ERROR) {
    _logger->Error("SO_UPDATE_ACCEPT_CONTEXT failed: {}", WSAGetLastError());
    closesocket(acceptedSocket);
    return ERROR;
}

// [필수] 게임 서버: Nagle 알고리즘 비활성화
BOOL nodelay = TRUE;
if (setsockopt(acceptedSocket, IPPROTO_TCP, TCP_NODELAY,
               (char*)&nodelay, sizeof(BOOL)) == SOCKET_ERROR) {
    _logger->Warn("TCP_NODELAY failed: {}", WSAGetLastError());
}

// [권장] Keepalive 활성화 및 커스터마이징
BOOL keepalive = TRUE;
setsockopt(acceptedSocket, SOL_SOCKET, SO_KEEPALIVE,
           (char*)&keepalive, sizeof(BOOL));

struct tcp_keepalive alive;
alive.onoff = TRUE;
alive.keepalivetime = 60000;      // 60초 (게임 서버용)
alive.keepaliveinterval = 10000;  // 10초
DWORD bytesReturned;
WSAIoctl(acceptedSocket, SIO_KEEPALIVE_VALS, &alive, sizeof(alive),
         NULL, 0, &bytesReturned, NULL, NULL);
```

---

### 설정 타이밍 가이드

| 타이밍 | 옵션 | 이유 |
|--------|------|------|
| **socket() 후** | 모든 옵션 가능 | 기본 설정 시점 |
| **bind() 전** | SO_EXCLUSIVEADDRUSE, SO_REUSEADDR | bind 시점에만 해석됨 |
| **listen() 전** | SO_RCVBUF (listen socket에) | TCP handshake 전에 설정되어야 상속됨 |
| **AcceptEx 완료 후** | SO_UPDATE_ACCEPT_CONTEXT | AcceptEx 전용, 즉시 설정 필수 |
| **accept() 후** | TCP_NODELAY, SO_KEEPALIVE | 연결별 설정 |
| **closesocket() 전** | SO_LINGER | 종료 동작 제어 |

---

### 게임 서버 특화 권장사항

#### 레이턴시 최적화
✅ **TCP_NODELAY = TRUE 필수**
- MMORPG는 작은 패킷 빈번히 전송
- Nagle 알고리즘은 레이턴시 증가 유발

✅ **SO_RCVBUF/SO_SNDBUF 기본값 사용**
- IOCP는 자체 버퍼 관리
- 커널의 자동 조정이 가장 효율적

#### 연결 관리
✅ **SO_KEEPALIVE + 커스텀 타임아웃**
- 기본 2시간은 너무 길음
- 권장: 60초 첫 keepalive, 10초 재시도 간격

✅ **애플리케이션 레벨 heartbeat (더 효과적)**
- 게임 로직에 통합 가능
- 더 빠른 감지 (30초 단위 권장)
- 네트워크 상태 모니터링 가능

#### 보안
✅ **SO_EXCLUSIVEADDRUSE 필수**
- 모든 서버는 반드시 설정
- 포트 하이재킹 방지

⛔ **SO_REUSEADDR 절대 사용 금지**
- Windows에서는 보안 위험
- 활성 소켓 포트 탈취 가능

#### 종료 처리
✅ **shutdown(SD_SEND) + closesocket()**
- SO_LINGER보다 명시적
- Graceful shutdown 보장

⚠️ **Hard close는 특수 케이스만**
- 악의적 클라이언트 강제 종료
- DoS 공격 대응

---

### 주의사항

#### 헤더 파일 순서 (중요!)

```cpp
#include <WinSock2.h>    // 필수: 먼저 include
#include <WS2tcpip.h>    // TCP/IP 옵션
#include <MSWSock.h>     // AcceptEx, IOCP 확장
// Windows.h는 WinSock2.h 이후에 include
```

**잘못된 순서**:
```cpp
#include <Windows.h>     // ❌ Windows.h가 먼저 오면
#include <WinSock2.h>    // ❌ Winsock 1.1 정의가 충돌
```

#### 에러 처리

```cpp
if (setsockopt(socket, level, optname, optval, optlen) == SOCKET_ERROR) {
    int error = WSAGetLastError();

    switch (error) {
        case WSAENOPROTOOPT:  // 10042
            // 옵션 미지원 또는 헤더 파일 불일치
            _logger->Error("Socket option not supported: {}", optname);
            break;

        case WSAEINVAL:       // 10022
            // 잘못된 옵션 값
            _logger->Error("Invalid socket option value");
            break;

        case WSAENOTSOCK:     // 10038
            // 유효하지 않은 소켓 핸들
            _logger->Error("Invalid socket handle");
            break;

        default:
            _logger->Error("setsockopt failed: {}", error);
            break;
    }
}
```

#### IOCP 워커 스레드 블로킹 방지

⚠️ **절대 사용 금지**:
```cpp
// ❌ 워커 스레드에서 블로킹 옵션 사용 금지
linger linger;
linger.l_onoff = 1;
linger.l_linger = 30;  // 30초 블로킹!
setsockopt(socket, SOL_SOCKET, SO_LINGER,
           (char*)&linger, sizeof(linger));
closesocket(socket);  // 최대 30초 블로킹
```

✅ **대신 사용**:
```cpp
// ✅ Non-blocking graceful shutdown
shutdown(socket, SD_SEND);  // 송신 종료 알림
closesocket(socket);        // 즉시 반환
```

---

## 현재 코드베이스 분석

### 사용 중인 소켓 옵션

#### ✅ Acceptor.cpp:161-166

```cpp
int result = setsockopt(
    overlapped->clientSocket,
    SOL_SOCKET,
    SO_UPDATE_ACCEPT_CONTEXT,
    reinterpret_cast<char*>(&_listenSocket),
    sizeof(_listenSocket));
```

**분석**: AcceptEx 사용 시 필수 옵션을 올바르게 구현함.

**개선 여부**: ✅ 적절함

---

#### ⚠️ Client.cpp:67-77 (주석 처리됨)

```cpp
//linger linger{ 0, 0 };
//if (isFireAndForget) {
//    linger.l_onoff = 1;
//}
//setsockopt(socket, SOL_SOCKET, SO_LINGER,
//           (char*)&linger, sizeof(linger));
```

**분석**: Hard close 로직이 주석 처리되어 있음. 현재는 기본 graceful close 사용.

**개선 여부**: ✅ 기본 동작이 적절함 (필요 시 주석 해제 가능)

---

#### ❌ WindowsAsyncSocket.cpp / SocketHelper.cpp

**분석**: Listening socket 생성 시 **아무 옵션도 설정하지 않음**.

**심각도**: 🔴 **긴급** - 보안 위험

**누락된 옵션**:
1. **SO_EXCLUSIVEADDRUSE** - 포트 하이재킹 방지 (필수)
2. **TCP_NODELAY** - 게임 서버 레이턴시 최적화 (필수)
3. **SO_KEEPALIVE** - 연결 관리 (권장)

---

### 개선 권장사항

#### 🔴 긴급: 보안 강화 (SO_EXCLUSIVEADDRUSE)

**파일**: `/home/user/highp-mmorpg/network/WindowsAsyncSocket.cpp`
**함수**: `Bind()` 내부, `bind()` 호출 전

```cpp
WindowsAsyncSocket::Res WindowsAsyncSocket::Bind(unsigned short port) {
    if (!IsValidPort(port)) {
        return Res::Ok();
    }

    // ============ 추가 시작 ============
    // [필수] 보안: 포트 하이재킹 방지
    BOOL exclusive = TRUE;
    if (setsockopt(_socketHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   (char*)&exclusive, sizeof(BOOL)) == SOCKET_ERROR) {
        DWORD error = WSAGetLastError();
        _logger->Error("Failed to set SO_EXCLUSIVEADDRUSE. error: {}", error);
        // 보안 옵션 설정 실패는 치명적이므로 에러 반환 고려
        return Res::Err(err::ESocketError::SetSocketOptFailed);
    }
    _logger->Debug("SO_EXCLUSIVEADDRUSE enabled for port {}", port);
    // ============ 추가 끝 ============

    _sockaddr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };

    // ... 기존 코드 계속
}
```

---

#### 🟠 중요: 레이턴시 최적화 (TCP_NODELAY)

**파일**: `/home/user/highp-mmorpg/echo/echo-server/EchoServer.cpp`
**함수**: `OnAccept()` 콜백 또는 Client 생성 시

```cpp
void EchoServer::OnAccept(network::AcceptContext& ctx) {
    // SO_UPDATE_ACCEPT_CONTEXT는 Acceptor에서 이미 처리됨

    // ============ 추가 시작 ============
    // [필수] 게임 서버: Nagle 알고리즘 비활성화
    BOOL nodelay = TRUE;
    if (setsockopt(ctx.acceptSocket, IPPROTO_TCP, TCP_NODELAY,
                   (char*)&nodelay, sizeof(BOOL)) == SOCKET_ERROR) {
        _logger->Warn("Failed to set TCP_NODELAY. error: {}", WSAGetLastError());
        // 경고만 하고 계속 진행 (치명적이지 않음)
    } else {
        _logger->Debug("TCP_NODELAY enabled for socket {}", ctx.acceptSocket);
    }

    // [권장] Keepalive 설정
    BOOL keepalive = TRUE;
    if (setsockopt(ctx.acceptSocket, SOL_SOCKET, SO_KEEPALIVE,
                   (char*)&keepalive, sizeof(BOOL)) == SOCKET_ERROR) {
        _logger->Warn("Failed to enable SO_KEEPALIVE. error: {}", WSAGetLastError());
    } else {
        // Keepalive 타임아웃 커스터마이징
        struct tcp_keepalive alive;
        alive.onoff = TRUE;
        alive.keepalivetime = 60000;      // 60초
        alive.keepaliveinterval = 10000;  // 10초
        DWORD bytesReturned;

        if (WSAIoctl(ctx.acceptSocket, SIO_KEEPALIVE_VALS, &alive, sizeof(alive),
                     NULL, 0, &bytesReturned, NULL, NULL) == SOCKET_ERROR) {
            _logger->Warn("Failed to set keepalive values. error: {}", WSAGetLastError());
        } else {
            _logger->Debug("Keepalive configured: 60s timeout, 10s interval");
        }
    }
    // ============ 추가 끝 ============

    // 기존 Client 생성 및 IOCP 연결 로직...
    auto client = FindAvailableClient();
    // ...
}
```

---

#### 🟡 선택적: Graceful Shutdown 개선

**파일**: `/home/user/highp-mmorpg/network/Client.cpp`
**함수**: `Close()`

```cpp
void Client::Close(bool isFireAndForget) {
    if (socket == INVALID_SOCKET) return;

    if (isFireAndForget) {
        // Hard close: 즉시 연결 종료 (악의적 클라이언트용)
        linger linger{0, 0};
        linger.l_onoff = 1;
        linger.l_linger = 0;

        if (setsockopt(socket, SOL_SOCKET, SO_LINGER,
                       (char*)&linger, sizeof(linger)) == SOCKET_ERROR) {
            _logger->Warn("Failed to set SO_LINGER for hard close: {}",
                          WSAGetLastError());
        }
    } else {
        // Graceful close: 명시적 shutdown 선호
        shutdown(socket, SD_SEND);  // 송신 종료 알림
        // 참고: recv()로 클라이언트의 FIN을 대기하는 로직 추가 가능
        // 하지만 IOCP 환경에서는 복잡도 증가로 보통 생략
    }

    closesocket(socket);
    socket = INVALID_SOCKET;
}
```

---

#### 🟢 선택적: Configuration 추가

**파일**: `/home/user/highp-mmorpg/echo/echo-server/config.runtime.toml`

```toml
[network.options]
# 소켓 옵션 설정
tcp_nodelay = true              # Nagle 알고리즘 비활성화 (게임 서버 필수)
keepalive_enabled = true        # Keepalive 활성화
keepalive_time_ms = 60000       # 60초 후 첫 keepalive 패킷
keepalive_interval_ms = 10000   # 10초 간격으로 재시도
exclusive_addr_use = true       # 포트 하이재킹 방지 (보안 필수)
```

**NetworkCfg.h에 추가**:
```cpp
struct SocketOptions {
    bool tcpNodelay = true;
    bool keepaliveEnabled = true;
    int keepaliveTimeMs = 60000;
    int keepaliveIntervalMs = 10000;
    bool exclusiveAddrUse = true;
};

struct NetworkCfg {
    // ... 기존 필드들
    SocketOptions socketOptions;
};
```

---

## 요약: IOCP 게임 서버 필수 체크리스트

### ⭐ 필수 설정 (즉시 구현)

- [x] **SO_UPDATE_ACCEPT_CONTEXT**: Acceptor.cpp에 구현됨 ✅
- [ ] **SO_EXCLUSIVEADDRUSE**: 미구현 - **즉시 추가 필요** 🔴
- [ ] **TCP_NODELAY**: 미구현 - **게임 서버 필수** 🟠

### ⭐ 강력 권장

- [ ] **SO_KEEPALIVE + SIO_KEEPALIVE_VALS**: 미구현 🟡
- [ ] **명시적 shutdown() 호출**: 부분 구현 (주석 처리됨) 🟡

### ❌ 사용 금지

- [x] **SO_REUSEADDR**: 사용 안 함 (올바름) ✅
- [x] **SO_LINGER (blocking)**: 주석 처리됨 (올바름) ✅

### 선택적

- [ ] **SO_RCVBUF/SO_SNDBUF**: 기본값 사용 중 (IOCP에서는 적절) ✅
- [ ] **IP_TTL/멀티캐스트 옵션**: 필요 시 추가

---

## 참고 자료

### Microsoft 공식 문서

- [SOL_SOCKET Socket Options](https://learn.microsoft.com/en-us/windows/win32/winsock/sol-socket-socket-options)
- [setsockopt function (Winsock2.h)](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-setsockopt)
- [IPPROTO_TCP socket options](https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-tcp-socket-options)
- [Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE](https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse)
- [Graceful Shutdown, Linger Options, and Socket Closure](https://learn.microsoft.com/en-us/windows/win32/winsock/graceful-shutdown-linger-options-and-socket-closure-2)
- [SO_KEEPALIVE socket option](https://learn.microsoft.com/en-us/windows/win32/winsock/so-keepalive)
- [SIO_KEEPALIVE_VALS Control Code](https://learn.microsoft.com/en-us/windows/win32/winsock/sio-keepalive-vals)
- [AcceptEx function (MSWSock.h)](https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nf-mswsock-acceptex)

### 추천 읽기

- **Nagle's Algorithm Analysis**: [It's the latency, stupid](http://www.stuartcheshire.org/papers/LatencyQuest.pdf)
- **TCP/IP Illustrated, Volume 1** (W. Richard Stevens) - TCP 옵션 상세 설명
- **Network Programming for Microsoft Windows** - Winsock 옵션 완벽 가이드

---

**문서 버전**: 1.0
**최종 수정**: 2026-01-20
**작성자**: Claude Code Analysis Team

# 자원 해제 / memory leak / 소유권 이상 분석 보고서

작성일: 2026-04-06  
범위: `network/`, `lib/`, `exec/chat/`, `exec/echo/`  
목적: 자원 해제 누락, memory leak, 소유권 이상(특히 `shared_ptr`/raw handle/재사용 객체) 정적 분석

## Requirements Summary

- 소스 변경 없이 현재 코드 기준으로 누수/소유권 이상을 정적 분석한다.
- 특히 아래를 우선 본다.
  - `SOCKET` / `HANDLE` / `AcceptEx` / IOCP / `WSAStartup` 수명
  - `shared_ptr` 기반 객체의 해제 경로
  - 재사용되는 `Client` 풀의 상태 초기화
  - 서버 종료 순서와 해제 순서

## Acceptance Criteria

- 각 핵심 이슈는 `file:line` 근거를 포함한다.
- 이슈를 `confirmed` / `suspicious`로 구분한다.
- 단순 스타일 이슈가 아니라 실제 수명/해제/소유권 영향이 있는 것만 우선순위화한다.
- 마지막에 수정 우선순위와 검증 절차를 제시한다.

## 핵심 발견 사항

### 1) [Critical][confirmed] disconnect 후 `User -> Session -> Client` 체인이 남아 객체가 해제되지 않고, 같은 `Client` 슬롯이 새 연결에 재사용될 수 있음

**근거**
- `GameLoop::Disconnect()`는 방에서만 제거하고 세션만 제거한다. `UserManager`에서는 제거하지 않는다.  
  - `exec/chat/chat-server/chat-server/GameLoop.cpp:128-137`
- `UserManager`는 `std::unordered_map<uint64_t, std::shared_ptr<User>> _users`로 `User`를 강하게 보관한다.  
  - `exec/chat/chat-server/chat-server/UserManager.h:37-42`
- `UserManager::CreateUser()`는 생성한 `User`를 `_users`에 넣는다.  
  - `exec/chat/chat-server/chat-server/UserManager.cpp:12-40`
- `User`는 `std::shared_ptr<Session>`를 가진다.  
  - `exec/chat/chat-server/chat-server/User.h:30-34`
- `Session`은 `std::shared_ptr<Client>`를 가진다.  
  - `exec/chat/chat-server/chat-server/Session.h:21-24`
- `ServerLifeCycle`는 `Client` 풀을 재사용한다. 사용 가능 조건은 `socket == INVALID_SOCKET` 뿐이다.  
  - `network/src/ServerLifecycle.cpp:109-113`
  - `network/src/ServerLifecycle.cpp:268-275`
- `Client::Close()`는 소켓만 닫고 `socket = INVALID_SOCKET`로 되돌린다.  
  - `network/src/client/windows/Client.cpp:127-150`
- 이후 `SetupClient()`가 같은 `Client` 객체에 새 `acceptSocket`을 다시 대입한다.  
  - `network/src/ServerLifecycle.cpp:181-203`

**왜 위험한가**
- disconnect 후에도 `UserManager`가 `User`를 계속 잡고 있으므로 `User`, `Session`, `Client` 객체가 살아남는다.
- 그런데 `Client` 풀은 `socket == INVALID_SOCKET`만 보면 “빈 슬롯”으로 간주해서 같은 `Client` 객체를 새 연결에 재사용한다.
- 그 결과 **이전 유저/세션이 새 연결의 `Client` 객체를 가리키는 aliasing**이 발생할 수 있다.
- 이 상태에서는 stale user/session이 새 소켓으로 송신하거나, 새 연결을 이전 유저로 잘못 식별할 여지가 생긴다.

**판정**
- confirmed

---

### 2) [High][confirmed] `Client` 콜백이 `ServerLifeCycle* this`를 캡처해서, `Client`가 lifecycle보다 오래 살면 use-after-free 가능

**근거**
- `ServerLifeCycle::BuildClientCallbacks()`는 `[this]` 캡처 람다를 `Client`에 심는다.  
  - `network/src/ServerLifecycle.cpp:19-55`
- `Client::Close()` / `Client::~Client()`는 disconnect 이벤트를 방출한다.  
  - `network/src/client/windows/Client.cpp:11-13`
  - `network/src/client/windows/Client.cpp:127-131`
- `ServerLifeCycle::Stop()`는 `_clientPool.clear()` 전에 `_acceptor`, `_iocp`를 파기하지만, 외부가 잡고 있는 `Client`까지 강제 정리하지는 않는다.  
  - `network/src/ServerLifecycle.cpp:126-139`
- chat 서버는 `Server::Stop()`에서 `_lifecycle`을 먼저 파기하고 `_gameLoop`를 나중에 파기한다.  
  - `exec/chat/chat-server/chat-server/Server.cpp:49-67`
- `GameLoop`가 들고 있는 `UserManager`/`SessionManager`가 살아있는 동안 `Client` strong ref가 남을 수 있다.  
  - `exec/chat/chat-server/chat-server/GameLoop.h:51-59`
  - `exec/chat/chat-server/chat-server/UserManager.h:37-42`
  - `exec/chat/chat-server/chat-server/Session.h:21-24`

**왜 위험한가**
- `Client` 내부 콜백은 이미 파기된 `ServerLifeCycle`의 멤버(`_connectedClientCount`, `_callbacks`)를 참조한다.
- 1번 이슈처럼 `UserManager`가 `Client`를 오래 살려두면, 나중에 `Client` 소멸/Close 시 stale callback이 실행될 수 있다.
- 즉 **해제 순서 + 강한 참조 체인 + raw `this` 캡처**가 합쳐져 UAF 가능성이 생긴다.

**판정**
- confirmed

---

### 3) [High][confirmed] 서버 종료 시 pending `AcceptEx`용 accept socket이 닫히지 않고 누수될 가능성이 큼

**근거**
- `PostAccept()`는 새 accept socket을 만들고, 성공하면 cleanup용 로컬 소유권을 끊고 `AcceptOverlapped.clientSocket`으로 소유권을 넘긴다.  
  - `network/src/acceptor/IocpAcceptor.cpp:92-111`
  - `network/src/acceptor/IocpAcceptor.cpp:147-149`
- 정상 완료 경로에서는 `OnAcceptComplete()`가 `clientSocketCleanup`으로 닫을 준비를 하고, 성공 시 callback 후 `InvalidSocket`로 비워서 소유권을 넘긴다.  
  - `network/src/acceptor/IocpAcceptor.cpp:174-191`
  - `network/src/acceptor/IocpAcceptor.cpp:217-226`
- 종료 경로 `Shutdown()`은 pending accept들에 대해 `CancelIoEx()`만 호출한다.  
  - `network/src/acceptor/IocpAcceptor.cpp:241-253`
- IOCP worker는 `ERROR_OPERATION_ABORTED`면 completion 처리 없이 루프를 빠져나간다.  
  - `network/src/io/windows/IocpIoMultiplexer.cpp:104-117`
- `AcceptOverlapped` holder가 파기될 때 실행되는 cleanup은 “object pool 반환”뿐이며 `clientSocket`을 닫는 로직이 아니다.  
  - `lib/inc/scope/DeferredItemHolder.hpp:11-27`
  - `lib/inc/scope/DeferContext.hpp:21-31`
  - `lib/inc/memory/HybridObjectPool.hpp:53-95`

**왜 위험한가**
- `AcceptEx`가 취소되어도 `OnAcceptComplete()`가 실행되지 않으면 `overlapped->clientSocket`을 닫아줄 코드가 없다.
- 그 accept socket은 풀로 돌아가는 `AcceptOverlapped` 내부에 남은 채로 소실될 수 있다.
- 즉 **서버 종료 시 in-flight accept 소켓 leak** 가능성이 높다.

**판정**
- confirmed

---

### 4) [High][confirmed] 재사용되는 `Client`의 `FrameBuffer`가 disconnect 시 초기화되지 않아 다음 연결로 데이터가 새어갈 수 있음

**근거**
- `Client`는 `FrameBuffer _frameBuf`를 멤버로 가진다.  
  - `network/inc/client/windows/Client.h:97-102`
- `FrameBuffer`는 내부 `_len`과 `_buf`를 유지하지만 reset API가 없다.  
  - `network/inc/client/FrameBuffer.h:27-30`
  - `network/src/client/FrameBuffer.cpp:25-43`
- `Client::Close()`는 소켓만 닫고 `_frameBuf`를 건드리지 않는다.  
  - `network/src/client/windows/Client.cpp:127-150`
- `ServerLifeCycle`는 같은 `Client` 객체를 풀에서 재사용한다.  
  - `network/src/ServerLifecycle.cpp:109-113`
  - `network/src/ServerLifecycle.cpp:181-203`
  - `network/src/ServerLifecycle.cpp:268-275`

**왜 위험한가**
- 이전 연결에서 partial frame이 남아 있으면 다음 연결이 같은 `Client` 객체를 재사용할 때 stale bytes를 이어받는다.
- 이는 엄밀한 “heap leak”은 아니지만, **연결 간 상태 누수(state retention / data bleed)** 이다.
- 패킷 검증 실패, 잘못된 디스패치, 심하면 다른 클라이언트의 데이터 잔재 노출로 이어질 수 있다.

**판정**
- confirmed

---

### 5) [Medium][confirmed] chat 서버의 `_hasStopped`가 초기화되지 않아 destructor cleanup 경로가 불안정함

**근거**
- `Server` 멤버 `std::atomic<bool> _hasStopped;`에 초기값이 없다.  
  - `exec/chat/chat-server/chat-server/Server.h:49-54`
- destructor는 `_hasStopped.load()`가 false일 때만 `Stop()`을 호출한다.  
  - `exec/chat/chat-server/chat-server/Server.cpp:18-21`
- `Stop()`이 끝나고 나서야 `_hasStopped`를 true로 바꾼다.  
  - `exec/chat/chat-server/chat-server/Server.cpp:49-52`

**왜 위험한가**
- 생성 직후 `_hasStopped`가 false라고 보장되지 않으면 destructor에서 `Stop()`이 건너뛰어질 수 있다.
- 그 경우 `_lifecycle`, `_gameLoop`, 내부 스레드/소켓 정리가 누락될 수 있다.

**판정**
- confirmed

---

### 6) [Medium][suspicious] listen socket 수명이 API 레벨에서 명시적으로 소유되지 않음

**근거**
- `ServerLifeCycle::Start()`는 `std::shared_ptr<ISocket> listenSocket`을 받지만 raw handle만 `IocpAcceptor`에 넘기고 저장하지 않는다.  
  - `network/src/ServerLifecycle.cpp:74-107`
- `IocpAcceptor`도 `SocketHandle _listenSocket`만 보관한다.  
  - `network/inc/acceptor/IocpAcceptor.h:134-149`
- `WindowsAsyncSocket::~WindowsAsyncSocket()`는 `Cleanup()`에서 `closesocket()` + `WSACleanup()`를 수행한다.  
  - `network/src/socket/WindowsAsyncSocket.cpp:12-14`
  - `network/src/socket/WindowsAsyncSocket.cpp:77-85`
- chat 서버 `Server`는 listen socket을 멤버로 저장하지 않는다.  
  - `exec/chat/chat-server/chat-server/Server.h:45-54`
  - `exec/chat/chat-server/chat-server/Server.cpp:28-46`
- echo 서버는 `_listenSocket` 멤버가 있지만 `Start()`에서 실제로 저장하지 않는다.  
  - `exec/echo/echo-server/Server.h:65-77`
  - `exec/echo/echo-server/Server.cpp:17-30`

**왜 위험한가**
- 현재 `main()` 지역변수가 살아 있어서 당장 깨지지 않지만, API 사용자가 `Start()` 후 listen socket shared_ptr를 내려놓으면 raw handle만 남는다.
- 그 순간 `WindowsAsyncSocket` 소멸이 listen socket과 WSA를 닫아버려 acceptor/lifecycle이 dangling handle을 쓰게 된다.

**판정**
- suspicious (현 call site는 안전하지만 API 계약은 취약)

## 상대적으로 괜찮아 보인 부분

- client-side `std::jthread` 종료/`join()` 경로는 비교적 정리되어 있다.  
  - `exec/chat/chat-client/Client.cpp:46-68`
  - `lib/inc/thread/LogicThread.cpp:5-15`
- metrics 쪽 `HANDLE`은 `DeferContext`로 닫고 있고 writer thread도 `Stop()`에서 join한다.  
  - `lib/src/metrics/ServerMetricsSupport.cpp:143-152`
  - `lib/src/metrics/ServerMetricsWriter.cpp:123-147`

## Implementation Steps

1. **disconnect 경로에서 `UserManager` 정리 추가**
   - `UserManager::RemoveByClient(...)`를 만들고 `GameLoop::Disconnect()`에서 `RoomManager`/`SessionManager`와 함께 호출.
   - 대상 파일:
     - `exec/chat/chat-server/chat-server/UserManager.h`
     - `exec/chat/chat-server/chat-server/UserManager.cpp`
     - `exec/chat/chat-server/chat-server/GameLoop.cpp`

2. **재사용 `Client`의 상태 reset 보장**
   - `Client::Close()` 또는 `SetupClient()` 전에 `_frameBuf`, 연결 플래그, 필요 시 send/recv state를 reset.
   - 대상 파일:
     - `network/inc/client/FrameBuffer.h`
     - `network/src/client/FrameBuffer.cpp`
     - `network/src/client/windows/Client.cpp`
     - `network/src/ServerLifecycle.cpp`

3. **`Client` 콜백에서 raw `this` 수명 의존 제거**
   - 옵션 A: `Client`가 lifecycle callback을 끊는 `ClearCallbacks()` 제공 후 `Stop()`에서 먼저 모든 client callback 제거
   - 옵션 B: callback이 `weak_ptr`/독립 counter sink를 보도록 구조 변경
   - 대상 파일:
     - `network/inc/client/windows/Client.h`
     - `network/src/client/windows/Client.cpp`
     - `network/src/ServerLifecycle.cpp`

4. **accept 취소 경로에서 accept socket 닫기**
   - `Shutdown()`에서 holder 내부 pending `AcceptOverlapped`마다 `clientSocket`까지 정리하거나,
   - `AcceptOverlapped` 반환 직전 socket-close defer를 추가.
   - 대상 파일:
     - `network/src/acceptor/IocpAcceptor.cpp`
     - 필요 시 `lib/inc/scope/DeferredItemHolder.hpp`

5. **listen socket ownership을 명시적으로 보존**
   - `ServerLifeCycle` 또는 상위 `Server`가 `std::shared_ptr<ISocket>`를 멤버로 보관.
   - echo/chat 둘 다 동일한 계약으로 맞춤.
   - 대상 파일:
     - `network/inc/server/ServerLifecycle.h`
     - `network/src/ServerLifecycle.cpp`
     - `exec/chat/chat-server/chat-server/Server.h`
     - `exec/chat/chat-server/chat-server/Server.cpp`
     - `exec/echo/echo-server/Server.cpp`

6. **초기화 누락 제거**
   - chat `Server::_hasStopped`를 `{false}`로 초기화.
   - 대상 파일:
     - `exec/chat/chat-server/chat-server/Server.h`

## Risks and Mitigations

- **Risk:** disconnect 정리 순서를 바꾸면 기존 metrics/callback 카운트가 달라질 수 있음  
  **Mitigation:** disconnect 시나리오별 연결 수/세션 수/user 수를 로그나 metrics로 함께 검증

- **Risk:** callback 제거 시 기존 통계 이벤트가 일부 사라질 수 있음  
  **Mitigation:** lifecycle 종료 단계와 client 종료 단계를 분리해 마지막 이벤트만 명시적으로 발행

- **Risk:** accept shutdown 정리 수정 시 double-close 우려  
  **Mitigation:** `InvalidSocket` sentinel을 일관되게 사용하고 close 후 즉시 무효화

## Verification Steps

1. 정적 검증
   - disconnect 후 `UserManager::_users`, `SessionManager::_sessions`, `_connectedClientCount`가 모두 감소하는지 코드 경로 확인
   - lifecycle stop 전후에 `Client` callback이 stale `this`를 참조하지 않는지 확인
   - accept shutdown 시 pending `AcceptOverlapped.clientSocket`가 모두 `InvalidSocket`이 되는지 확인

2. 수동 런타임 검증 (Windows)
   - chat server 실행 후 join/leave/disconnect를 반복해서 user/session count가 누적되지 않는지 확인
   - 클라이언트 강제 종료 후 동일 서버에 재접속했을 때 이전 user/session이 재사용 client를 오염시키지 않는지 확인
   - 서버 종료 직전 backlog만큼 접속 시도 후 종료하여 핸들 수가 증가한 채 남지 않는지 확인

3. 계측 권장
   - `UserManager` / `SessionManager` / `RoomManager` / `ClientPool` current size 로그 추가
   - 종료 직전 pending accept 수, connected client 수, queued packet 수를 출력

## 우선순위

1. `GameLoop::Disconnect()` + `UserManager` 정리 누락 수정
2. `Client` callback raw `this` 캡처 제거
3. `AcceptEx` shutdown 누수 수정
4. `Client` 재사용 reset 보강
5. listen socket ownership 명시화
6. `_hasStopped` 초기화

## Not-tested

- Windows 런타임에서 실제 핸들 수/메모리 사용량 계측은 수행하지 못함
- 본 보고서는 정적 분석 기반이며, manual integration test는 아직 미실행

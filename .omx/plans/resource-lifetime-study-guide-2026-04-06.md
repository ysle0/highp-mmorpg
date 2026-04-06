# Resource / Lifetime Study Guide — 2026-04-06

## 문서 목적

이 문서는 아래 3가지를 한 번에 정리하기 위한 학습용 문서다.

1. 이번 코드베이스에서 발견한 **자원 해제 / memory leak / ownership / lifetime 문제 1~7번**
2. 앞으로 비슷한 실수를 줄이기 위해 알아야 할 **개념 / 마인드셋**
3. 실제로 코드를 고칠 때 따라갈 **수정 순서 체크리스트**

대상 코드베이스:
- `network/`
- `exec/chat/`
- `exec/echo/`
- 관련 `lib/` 유틸

관련 기존 문서:
- 감사 보고서: `.omx/plans/resource-lifetime-audit-2026-04-06.md`
- 수정 계획: `.omx/plans/resource-fix-plan-2026-04-06.md`

---

# 1. 먼저 큰 그림: 이번 문제들의 공통 뿌리

이번에 본 문제 1~7은 각각 따로 떨어진 버그처럼 보이지만, 실제로는 아래 3개의 공통 원인에서 파생된다.

## 1) 소유권 경계가 불명확하다

예:
- 누가 `Client`를 “진짜로” 소유하는가?
- `Client`가 재사용 가능해지는 조건은 누가 판단하는가?
- `User`, `Session`, `Client` 중 무엇이 무엇을 살려두는가?

문제는 코드가 “어느 정도는” shared ownership을 쓰고 있지만,  
**최종 해제 책임자**가 명확하게 문서화/강제되어 있지 않다는 점이다.

---

## 2) 정상 경로와 비정상 경로가 다른 teardown을 쓴다

예:
- graceful disconnect는 `OnDisconnect -> GameLoop::Disconnect`
- malformed packet는 `client->Close(true)` 직행

즉:
- 어떤 경로는 app cleanup이 되고
- 어떤 경로는 socket만 닫히고 끝난다

이런 구조에서는 resource 정리가 “운 좋으면 되는” 코드가 된다.

---

## 3) 비동기 I/O의 in-flight 상태를 명시적으로 보호하지 않는다

예:
- recv overlapped 하나를 누가 repost하는지 불명확
- send buffer 하나를 여러 send가 덮어쓸 수 있음

비동기 I/O에서는 “함수 호출이 끝났다”가 “작업이 끝났다”가 아니다.

즉:
- `WSARecv` 호출 후에도 recv buffer/overlapped는 아직 사용 중일 수 있고
- `WSASend` 호출 후에도 send buffer는 completion 전까지 살아 있어야 한다

이 사실을 코드 구조가 강제하지 않으면, 언젠가 반드시 꼬인다.

---

# 2. 문제 1~7 상세 설명

---

## 문제 1) Chat disconnect 후 `User -> Session -> Client`가 남는 문제

### 근거 코드
- `exec/chat/chat-server/chat-server/GameLoop.cpp:128-137`
- `exec/chat/chat-server/chat-server/UserManager.h:38-42`
- `exec/chat/chat-server/chat-server/UserManager.cpp:31-40`
- `exec/chat/chat-server/chat-server/User.h:30-34`
- `exec/chat/chat-server/chat-server/Session.h:21-24`
- `network/src/client/windows/Client.cpp:127-150`
- `network/src/ServerLifecycle.cpp:268-276`

### 현재 무슨 일이 일어나나

disconnect 시 현재 cleanup은 대략 이렇다:

1. room에서 끊긴 client를 kick
2. session 제거
3. **user는 제거 안 함**

그런데 `UserManager`는 `shared_ptr<User>`를 들고 있고,  
`User`는 `shared_ptr<Session>`,  
`Session`은 `shared_ptr<Client>`를 들고 있다.

즉 disconnect 후에도 아래 체인이 남을 수 있다:

`UserManager::_users -> User -> Session -> Client`

### 진짜 원인

**disconnect cleanup 그래프가 불완전**하기 때문이다.

즉 “연결 종료”라는 이벤트가 왔을 때,
이 연결을 참조하는 모든 owner를 끊지 않는다.

### 왜 위험한가

network 레이어는 `socket == INVALID_SOCKET`이면 해당 `Client`를 재사용 가능한 슬롯으로 본다.

그래서:
- app 레이어는 예전 `Client`를 붙잡고 있고
- network 레이어는 그 같은 `Client` 객체를 새 연결에 재사용할 수 있다

즉 단순 leak이 아니라:
- stale state
- 잘못된 세션 참조
- connection identity 오염
까지 이어질 수 있다.

### 핵심 교훈

`shared_ptr`를 쓴다고 자동으로 안전해지지 않는다.

오히려:
- “누가 살아남게 만드는가”
- “언제 마지막 owner가 사라져야 하는가”

를 더 엄격하게 생각해야 한다.

### 수정 방향

disconnect 시 최소한 아래가 모두 정리되어야 한다:

1. Room membership
2. User ownership
3. Session ownership
4. Client transport state

즉 **연결 teardown의 단일 경로**를 만들어야 한다.

---

## 문제 2) Server shutdown 시 active client를 명시적으로 close/drain 하지 않는 문제

### 근거 코드
- `exec/chat/chat-server/chat-server/Server.cpp:49-61`
- `network/src/ServerLifecycle.cpp:126-139`

### 현재 무슨 일이 일어나나

현재 `ServerLifeCycle::Stop()`은 대략:

1. acceptor shutdown
2. iocp shutdown
3. `_clientPool.clear()`

여기서 빠진 것이 있다:

**active client를 명시적으로 close/drain 하는 단계**

### 진짜 원인

`_clientPool`을 “slot registry”이자 “ownership storage”처럼 동시에 쓰고 있기 때문이다.

하지만 이 둘은 다르다.

- registry에서 지운다 = 추적 목록에서 제거
- ownership이 끝난다 = 마지막 owner가 사라진다

`shared_ptr` 구조에서는 컨테이너를 clear해도 다른 owner가 있으면 객체는 계속 살아 있다.

### 왜 위험한가

1번 문제와 결합되면:
- lifecycle은 종료되었는데
- app 쪽이 `Client`를 계속 살려둘 수 있다

그 상태에서:
- socket/resource가 남을 수 있고
- callback lifetime도 꼬인다

### 핵심 교훈

`vector.clear()`는 cleanup의 “일부”일 뿐이다.  
절대 cleanup 그 자체로 착각하면 안 된다.

### 수정 방향

shutdown은 단계형이어야 한다:

1. stopping flag
2. 새 accept 중단
3. active client close
4. outstanding completion drain/cancel
5. worker infrastructure 종료
6. pool clear

---

## 문제 3) `AcceptEx` pending socket / overlapped shutdown semantics 문제

### 근거 코드
- `network/src/acceptor/IocpAcceptor.cpp:92-150`
- `network/inc/client/windows/OverlappedExt.h:51-57`
- `network/inc/acceptor/IocpAcceptor.h:148-149`
- `lib/inc/scope/DeferredItemHolder.hpp:11-27`
- `lib/inc/memory/HybridObjectPool.hpp:53-95`
- `network/src/acceptor/IocpAcceptor.cpp:241-254`
- `network/src/ServerLifecycle.cpp:126-134`

### 현재 무슨 일이 일어나나

`PostAccept()`는:
- accept용 socket을 새로 만들고
- 그 handle을 `AcceptOverlapped::clientSocket`에 저장하고
- pending 상태로 holder에 넣는다

정상 경로에서는:
- accept completion에서 socket을 서버 쪽으로 넘기고
- `clientSocket = InvalidSocket`

하지만 shutdown 경로에서는:
- `CancelIoEx()`만 하고
- `clientSocket`를 명시적으로 닫지 않는다

### 진짜 원인

하나의 구조체가 두 개의 수명을 들고 있는데,
그 중 하나만 제대로 관리하기 때문이다.

`AcceptOverlapped`는 실제로:

1. overlapped 메모리 lifetime
2. 내부 socket handle lifetime

을 동시에 가진다.

현재 코드는 1번은 object pool/holder로 관리하지만,
2번은 shutdown 경로에서 놓친다.

### 왜 위험한가

이건 memory leak보다 더 정확히는 **handle leak**이다.

게다가 stop 순서가:
- acceptor 먼저 파괴
- IOCP는 나중에 종료

라서 canceled completion이 늦게 올라오면 stale overlapped를 볼 가능성도 생긴다.

### 핵심 교훈

비동기 구조체 안에 메모리만 있는 게 아니다.  
그 안에 **OS resource ownership**이 숨어 있으면,
그것도 별도의 release path가 필요하다.

### 수정 방향

shutdown 시 held accept를 순회하며:
- cancel
- socket close
- ownership invalidate

를 명시적으로 해야 한다.

그리고 acceptor lifetime이 IOCP drain보다 먼저 끝나지 않게 순서를 조정해야 한다.

---

## 문제 4) Echo 서버가 같은 recv overlapped를 이중 게시하는 문제

### 근거 코드
- `network/src/ServerLifecycle.cpp:240-248`
- `exec/echo/echo-server/Server.cpp:57-60`
- `network/inc/client/windows/Client.h:91-95`

### 현재 무슨 일이 일어나나

recv completion 후:
- lifecycle이 `PostRecv()`를 다시 건다
- echo 서버도 또 `PostRecv()`를 건다

그런데 client는 recv overlapped를 하나만 가진다.

### 진짜 원인

**누가 recv repost owner인지 계약이 없기 때문**

즉:
- network도 “내가 repost”
- app도 “내가 repost”

라고 생각하고 있다.

### 왜 위험한가

overlapped I/O에서 overlapped 구조체는 단순한 data holder가 아니다.
그건 **in-flight operation의 상태 객체**다.

같은 recv overlapped로 동시에 두 개의 recv를 걸면:
- completion 대상이 충돌하고
- buffer 의미가 꼬이고
- undefined behavior가 생긴다

### 핵심 교훈

비동기 작업의 다음 단계 repost는 항상 **한 계층만** 책임져야 한다.

### 수정 방향

recv repost owner를 lifecycle 하나로 통일.
echo app에서는 중복 `PostRecv()` 제거.

---

## 문제 5) `Client` callbacks가 파괴된 `ServerLifeCycle`의 raw `this`를 잡는 문제

### 근거 코드
- `network/src/ServerLifecycle.cpp:19-55`
- `network/src/ServerLifecycle.cpp:66-70`
- `network/src/ServerLifecycle.cpp:111-114`

### 현재 무슨 일이 일어나나

`BuildClientCallbacks()`에서 `[this]`를 캡처한 lambda를 만들고,
그 콜백을 각 `Client`에 심는다.

즉 hidden edge는 이렇다:

`Client -> callback lambda -> raw this(ServerLifeCycle)`

### 진짜 원인

이 구조는 **Client가 반드시 ServerLifeCycle보다 먼저 죽는다**는 가정 위에서만 안전하다.

그런데 현재 코드는:
- client가 pooled/shared-owned이고
- app에서 leak/retention될 수 있으므로

그 가정이 깨진다.

### 왜 위험한가

retained client가 나중에 callback을 호출하면,
이미 죽은 lifecycle의 메모리를 건드릴 수 있다.

즉 leak이 곧 dangling callback/UAF의 씨앗이 된다.

### 핵심 교훈

callback은 ownership이 없어 보이지만,  
실제로는 **숨은 수명 의존성**을 만든다.

raw `this` 캡처는
“상대가 반드시 나보다 먼저 죽는다”는 보장이 있을 때만 안전하다.

### 수정 방향

1. 먼저 leaked client를 없애고
2. shutdown 전에 client callbacks를 clear/reset하고
3. 필요하면 weak/indirection/state object 방식으로 바꾸기

---

## 문제 6) per-client send in-flight ownership guard 부재

### 근거 코드
- `network/inc/client/windows/Client.h:94-95`
- `network/src/client/windows/Client.cpp:99-125`
- `exec/chat/chat-server/chat-server/Session.cpp:38-49`
- `exec/chat/chat-server/chat-server/room/Room.cpp:30-34`
- `exec/chat/chat-server/chat-server/room/Room.cpp:61-67`
- `exec/chat/chat-server/chat-server/room/Room.cpp:77-83`

### 현재 무슨 일이 일어나나

client당 send buffer/overlapped는 하나뿐인데,
`PostSend()`는 매번 그 버퍼를 다시 덮어쓴다.

만약 첫 번째 send completion 전에 두 번째 send가 오면,
같은 buffer가 덮어써질 수 있다.

### 진짜 원인

network layer가 “send는 겹치지 않을 것”이라는  
**암묵적 가정**에 의존하고 있기 때문이다.

하지만 그 규칙을:
- 코드가 강제하지도 않고
- queue로 보장하지도 않고
- guard flag로 막지도 않는다

### 왜 위험한가

비동기 send에서 buffer는 completion 전까지 살아 있어야 한다.

즉:
- 함수가 리턴했다고 그 버퍼가 자유로워진 것이 아님

### 핵심 교훈

async send에서는
**buffer ownership 기간 = API call 순간 ~ completion 시점**

이다.

### 수정 방향

가장 권장되는 방식:
- per-client single-flight send
- 나머지는 queue에 적재
- send completion에서 다음 send 발행

---

## 문제 7) invalid packet / frame error 경로가 app cleanup을 우회하는 문제

### 근거 코드
- `network/src/server/PacketDispatcher.cpp:23-29`
- `network/src/server/PacketDispatcher.cpp:46-55`
- `network/src/server/PacketDispatcher.cpp:80-86`
- `network/src/client/windows/Client.cpp:127-150`
- `exec/chat/chat-server/chat-server/GameLoop.cpp:128-137`

### 현재 무슨 일이 일어나나

malformed packet / oversize frame / invalid payload가 오면
`PacketDispatcher`가 직접 `client->Close(true)`를 호출한다.

그런데 app cleanup은 `GameLoop::Disconnect()`에 있다.

즉:
- socket은 닫히는데
- room/user/session cleanup은 안 될 수 있다

### 진짜 원인

**transport close와 app teardown을 같은 걸로 착각**하고 있기 때문이다.

하지만 실제로는 다르다:

- transport close = 소켓 끊기
- session teardown = app state 정리

### 왜 위험한가

비정상 입력 하나만으로도
정상 disconnect와 다른 cleanup path를 타게 된다.

그래서 malformed traffic이 leak/stale state를 유발할 수 있다.

### 핵심 교훈

강제 종료 경로도 반드시 정상 종료 경로와 같은 teardown policy로 수렴해야 한다.

### 수정 방향

`PacketDispatcher`가 직접 close authority를 갖지 않게 하고,
lifecycle/app teardown 경로로 위임해야 한다.

---

# 3. 앞으로 실수 안 하려면 알아야 할 것

## A. “Close”와 “Cleanup”은 다르다

항상 구분해야 한다:

- `closesocket()` 했다
- session/user/room/app state까지 정리했다

이 둘은 같은 말이 아니다.

### 기억 문장
> “transport close는 물리적 종료이고, cleanup은 논리적 종료다.”

---

## B. 모든 resource는 owner가 있어야 한다

각 리소스마다 반드시 물어봐야 한다:

1. 누가 생성하는가?
2. 누가 소유하는가?
3. 정상 경로에서 누가 해제하는가?
4. 실패 경로에서 누가 해제하는가?
5. shutdown 경로에서 누가 해제하는가?

이 질문에 답 못하면 거의 항상 leak 후보가 있다.

---

## C. async에서는 “지금 안 쓴다”가 “안전하다”가 아니다

비동기 작업에 들어간 자원은:
- 함수가 끝난 뒤에도
- completion 전까지는
- 여전히 커널/런타임/다른 스레드가 쓰고 있을 수 있다

대표 예:
- overlapped
- send buffer
- recv buffer
- completion key 대상 객체

---

## D. `shared_ptr`는 편한 대신 책임을 흐린다

`shared_ptr`가 많아지면 장점:
- 쓰기 편함
- 조기 파괴 방지

단점:
- 최종 owner가 누구인지 흐려짐
- “왜 안 죽지?” 디버깅이 어려움
- hidden retention이 생김

### 기억 문장
> “shared_ptr는 안전장치이기도 하지만, lifetime을 흐리는 안개이기도 하다.”

---

## E. raw callback 캡처는 수명 계약이다

`[this]`를 캡처했다면 무조건 생각해야 한다:

> “이 콜백이 살아 있는 동안, 나는 반드시 살아 있나?”

대답이 “아마도”면 unsafe다.

---

## F. unhappy path를 happy path와 따로 만들지 말자

가장 좋은 구조:
- graceful disconnect
- malformed packet disconnect
- shutdown disconnect

가 모두 결국 **하나의 canonical teardown path**로 합쳐지는 것

---

## G. slot reuse 조건은 “겉상태”만 보면 안 된다

지금 코드처럼:
- `socket == INVALID_SOCKET`이면 free slot

만으로 판단하면 위험하다.

왜냐면:
- app owner가 남았을 수 있고
- completion이 늦게 올 수 있고
- callback edge가 남았을 수 있기 때문이다

---

# 4. 가져야 할 마인드셋

## 마인드셋 1) “누가 마지막으로 책임지지?”를 습관처럼 묻기

새 코드를 볼 때마다:
- 이 객체의 마지막 owner는 누구지?
- 비정상 종료 시 누가 치우지?
- 이 자원은 파괴되기 전에 다른 스레드가 만질 수 있나?

를 자동으로 떠올려야 한다.

---

## 마인드셋 2) 정상 경로보다 실패 경로를 더 의심하기

대부분의 leak/UAF는 성공 경로보다:
- 에러 반환
- early return
- cancel
- shutdown
- malformed input

에서 생긴다.

### 기억 문장
> “성공 경로는 대체로 잘 짜여 있고, 실패 경로가 진짜 코드 품질을 드러낸다.”

---

## 마인드셋 3) teardown도 하나의 프로토콜이라고 생각하기

start 순서가 중요하듯
stop 순서도 중요하다.

예:
- accept stop
- active client close
- completion drain
- worker stop
- callback detach
- container clear

shutdown을 그냥 “마지막에 대충 닫기”로 생각하면 안 된다.

---

## 마인드셋 4) “지금은 운 좋게 된다”를 허용하지 않기

예:
- send가 겹치지 “않겠지”
- callback이 늦게 안 오겠지
- close하면 cleanup도 되겠지

이런 생각은 비동기/서버 코드에서 특히 위험하다.

### 기억 문장
> “서버 코드는 선의에 기대지 말고, 규칙을 코드로 강제해야 한다.”

---

## 마인드셋 5) 재사용 객체는 새것처럼 초기화된다는 보장이 있어야 한다

pool/pool-like slot 구조에서는 반드시 생각해야 한다:

- 이 객체가 이전 생애의 흔적을 남기나?
- callback/state/buffer/flag/counter가 완전히 reset되나?
- 늦은 completion이 새 생애에 섞일 수 있나?

---

# 5. 실제 코드 수정 순서 체크리스트

아래 순서대로 고치는 것을 권장한다.

---

## Phase 0 — 수정 전 안전 체크

- [ ] disconnect / forced-close / shutdown 경로를 종이에 그린다
- [ ] `Client`, `Session`, `User`, `Room`, `ServerLifeCycle`, `IocpAcceptor`의 owner 관계를 메모한다
- [ ] “어떤 경로가 canonical teardown path인지” 먼저 결정한다
- [ ] 같은 cleanup을 두 군데 이상 중복 구현하지 않겠다고 정한다

---

## Phase 1 — disconnect ownership 정리 (가장 먼저)

### 목표
정상 disconnect와 강제 disconnect가 모두 같은 app cleanup 경로를 타게 만들기

### 체크리스트
- [ ] `GameLoop::Disconnect()`가 user/session/room cleanup을 모두 담당하도록 설계 확정
- [ ] `UserManager`에서 client 기준 user 찾기 및 제거 경로 재확인
- [ ] disconnect 시 `UserManager`에서도 제거되도록 수정
- [ ] room에서 user 제거와 `UserManager` 제거 순서를 정함
- [ ] `SessionManager` 제거가 반드시 함께 일어나도록 정리
- [ ] malformed packet 경로가 direct `client->Close(true)`를 하지 않도록 변경 설계
- [ ] `PacketDispatcher`가 lifecycle/app에 disconnect 의도를 전달하게 변경

### 주로 수정할 파일
- [ ] `exec/chat/chat-server/chat-server/GameLoop.cpp`
- [ ] `exec/chat/chat-server/chat-server/UserManager.cpp`
- [ ] `exec/chat/chat-server/chat-server/room/Room.cpp`
- [ ] `network/src/server/PacketDispatcher.cpp`
- [ ] `network/src/ServerLifecycle.cpp`

### 완료 기준
- [ ] graceful disconnect 후 user/session/client 관련 카운트가 baseline 복귀
- [ ] malformed packet 후에도 동일하게 baseline 복귀

---

## Phase 2 — shutdown 순서 정리

### 목표
shutdown이 “자원 종료 프로토콜”이 되게 만들기

### 체크리스트
- [ ] `ServerLifeCycle::Stop()`에 stopping state 도입 여부 결정
- [ ] 새 accept 중단 단계 추가
- [ ] active client 순회 close 단계 추가
- [ ] client callback reset 시점 결정
- [ ] outstanding completion drain/cancel 정책 정함
- [ ] `_clientPool.clear()`를 가장 마지막 단계로 이동

### 주로 수정할 파일
- [ ] `network/src/ServerLifecycle.cpp`
- [ ] `network/inc/server/ServerLifecycle.h`
- [ ] `exec/chat/chat-server/chat-server/Server.cpp`

### 완료 기준
- [ ] active client 붙은 상태에서 stop해도 socket/resource 잔류 없음
- [ ] 파괴된 lifecycle로 callback이 날아오지 않음

---

## Phase 3 — AcceptEx shutdown semantics 정리

### 목표
pending accept의 socket handle과 overlapped memory가 둘 다 안전하게 정리되게 만들기

### 체크리스트
- [ ] held accept overlapped 순회 API 재확인
- [ ] shutdown 시 각 held item의 `clientSocket` close 추가
- [ ] close 후 `clientSocket = InvalidSocket` 보장
- [ ] normal completion path와 shutdown path가 ownership 중복 해제하지 않게 규칙 정함
- [ ] acceptor destruction과 IOCP drain의 순서 재조정

### 주로 수정할 파일
- [ ] `network/src/acceptor/IocpAcceptor.cpp`
- [ ] `network/inc/acceptor/IocpAcceptor.h`
- [ ] `network/src/ServerLifecycle.cpp`

### 완료 기준
- [ ] pending accept가 많은 상태에서 stop해도 handle leak 없음
- [ ] stale overlapped access 징후 없음

---

## Phase 4 — recv ownership 정리

### 목표
recv repost 책임자를 하나로 고정

### 체크리스트
- [ ] recv repost owner를 `ServerLifeCycle`로 명시
- [ ] app layer에서 direct `PostRecv()` 호출 제거
- [ ] 관련 주석/계약 문서 보강

### 주로 수정할 파일
- [ ] `exec/echo/echo-server/Server.cpp`
- [ ] 필요 시 `network/inc/server/ServerLifecycle.h`

### 완료 기준
- [ ] client당 outstanding recv가 1개만 유지됨

---

## Phase 5 — send ownership 정리

### 목표
send buffer/overlapped가 completion 전 덮어써지지 않게 만들기

### 체크리스트
- [ ] per-client in-flight send flag 추가 여부 결정
- [ ] send queue 도입 여부 결정
- [ ] send completion에서 다음 send를 발행하도록 설계
- [ ] queue overflow/backpressure 정책 정함

### 주로 수정할 파일
- [ ] `network/inc/client/windows/Client.h`
- [ ] `network/src/client/windows/Client.cpp`
- [ ] 필요 시 `network/src/ServerLifecycle.cpp`

### 완료 기준
- [ ] 같은 client에 rapid multi-send 해도 순서 보장
- [ ] send buffer corruption 없음

---

# 6. 실제 수정할 때 매번 확인할 질문

수정 중간마다 아래 질문을 반복해서 보면 실수가 크게 줄어든다.

## Ownership 질문
- [ ] 이 객체의 최종 owner는 누구인가?
- [ ] 지금 추가한 `shared_ptr`가 수명을 불필요하게 늘리지는 않는가?
- [ ] 컨테이너에서 제거되면 실제로 파괴되는가?

## Async 질문
- [ ] 이 buffer/overlapped는 completion 전까지 안정적으로 살아 있나?
- [ ] 같은 상태 객체를 중복 in-flight로 쓰고 있지 않나?
- [ ] cancel/shutdown 중 completion이 늦게 와도 안전한가?

## Shutdown 질문
- [ ] stop 순서가 start 순서만큼 명확한가?
- [ ] close와 cleanup이 분리되어 있나?
- [ ] abnormal path도 canonical teardown으로 수렴하나?

## Callback 질문
- [ ] 이 callback이 잡고 있는 대상은 callback lifetime보다 오래 사나?
- [ ] raw `this`를 정말 써도 되나?

---

# 7. 최종 요약

이번 코드에서 가장 먼저 고쳐야 하는 건 “메모리 free” 자체보다도,
**수명 모델을 일관되게 만드는 것**이다.

핵심은 아래 세 문장으로 요약된다.

1. **socket close와 app cleanup을 분리해서 생각하자**
2. **비정상 종료도 정상 종료와 같은 teardown path로 모으자**
3. **async I/O 상태 객체는 completion 전까지 절대 자유롭지 않다**

---

# 8. 다음 추천 액션

학습 다음 단계로 가장 좋은 흐름:

1. 이 문서를 보며 **disconnect canonical path 설계**
2. 그 다음 **Phase 1만 실제 패치**
3. 그 다음 **shutdown 정리**
4. 마지막에 **recv/send in-flight ownership 보강**

이 순서가 가장 안전하고, 원인 학습에도 좋다.

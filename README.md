# highp-mmorpg

Windows IOCP 기반 고성능 비동기 MMORPG 서버 백엔드

> C++20 · IOCP · Zero-copy Serialization · Lock-free Memory Pool

## Architecture

```
┌─────────────────────────────────────────┐
│            Application Layer            │
│     EchoServer · ChatServer · ...       │
├─────────────────────────────────────────┤
│             Network Layer               │
│  IocpIoMultiplexer ─ Acceptor ─ Client  │
├──────────────┬──────────────────────────┤
│   Protocol   │       Utilities          │
│  FlatBuffers │  ObjectPool · Logger · … │
└──────────────┴──────────────────────────┘
```

## IOCP (I/O Completion Port)

Windows 커널이 제공하는 비동기 I/O 통지 메커니즘. 소켓에 비동기 작업(AcceptEx, WSARecv, WSASend)을 요청하면, 커널이 완료 시 IOCP 큐에 결과를 적재하고, 워커 스레드가 `GetQueuedCompletionStatus`(GQCS)로 꺼내 처리한다.

```
          ┌──────────────────────────────────────────────────────┐
          │                    Kernel (IOCP)                     │
          │                                                      │
          │  AcceptEx / WSARecv / WSASend  ──►  Completion Queue │
          └──────────────────────┬───────────────────────────────┘
                                 │ GQCS
            ┌────────────────────┼────────────────────┐
            ▼                    ▼                    ▼
      Worker Thread 0      Worker Thread 1      Worker Thread N
```

- **논블로킹** — I/O 요청 후 스레드는 즉시 반환, 완료 시점에만 깨어남
- **스레드 풀 효율** — N개 워커가 하나의 IOCP 핸들을 공유, 커널이 스레드 스케줄링 최적화
- **확장성** — 수천 동시 연결을 소수의 워커 스레드로 처리 가능

## Threading Model

```
┌─────────────────────────────────────────────────────────────┐
│ Main Thread                                                 │
│  ServerLifeCycle::Start()                                   │
│  ├─ IocpIoMultiplexer::Initialize(N) ── Worker Threads 생성 │
│  ├─ IocpAcceptor::PostAccepts()      ── 비동기 Accept 등록  │
│  └─ LogicLoop() 또는 대기                                    │
├─────────────────────────────────────────────────────────────┤
│ IOCP Worker Threads (N개, std::jthread)                     │
│  GQCS 대기 → CompletionEvent 수신 → 타입별 분기:            │
│  ├─ Accept  → SetupClient → PostRecv                       │
│  ├─ Recv    → PacketDispatcher::Receive() → MPSC 큐 적재    │
│  └─ Send    → 완료 처리                                     │
├─────────────────────────────────────────────────────────────┤
│ Logic Thread (std::jthread, ChatServer 등)                  │
│  Tick 루프:                                                  │
│  └─ PacketDispatcher::Tick() → MPSC 큐에서 꺼내 핸들러 실행  │
└─────────────────────────────────────────────────────────────┘
```

- **Worker Thread**: 네트워크 I/O 완료 처리 전담. 비즈니스 로직 실행 금지.
- **Logic Thread**: 게임 로직 전담. `PacketDispatcher`가 MPSC 큐로 스레드 간 안전하게 전달.
- **EchoServer**: 단순 에코이므로 Logic Thread 없이 Worker에서 직접 `PostSend`.

## Packet Pipeline

```
Client Socket
  │
  ▼
WSARecv 완료 (Worker Thread)
  │
  ▼
FrameBuffer ── 프레임 조립 (길이 프리픽스 기반, 불완전 프레임 누적)
  │
  ▼
FlatBuffers Verify ── 패킷 무결성 검증
  │
  ▼
ParsePacket ── Packet* 역직렬화, Payload union 추출
  │
  ▼
MPSC Queue ── 페이로드 복사 후 커맨드 큐에 적재 (double-buffer swap + mutex)
  │
  ▼
Logic Thread Tick()
  │
  ▼
PacketDispatcher ── PayloadType → IPacketHandler<T> 디스패치
  │
  ├─► ChatMessageHandler::Handle()  ── 채팅 메시지 브로드캐스트
  ├─► JoinRoomHandler::Handle()     ── 방 입장 처리
  └─► ...
```

| 단계 | 스레드 | 클래스 |
|------|--------|--------|
| WSARecv 완료 | Worker | `ServerLifeCycle` |
| 프레임 조립 | Worker | `FrameBuffer` |
| 검증 + 파싱 | Worker | `PacketDispatcher::Receive()` |
| 큐 적재 | Worker → Logic | `MPSCCommandQueue` (double-buffer swap) |
| 핸들러 실행 | Logic | `IPacketHandler<T>` 구현체 |

## Highlights

- **IOCP async I/O** — `AcceptEx` 파이프라이닝, `GetQueuedCompletionStatus` 워커 풀, `std::jthread` + `stop_token` 기반 graceful shutdown
- **Memory pool 전략 4종** — Mutex 기반 / Thread-local / TLS-first Hybrid / Vyukov MPMC Lock-free, 벤치마크 포함
- **FlatBuffers 프로토콜** — 스키마 기반 zero-copy 직렬화, 채팅 프로토콜 구현 (로비/방/브로드캐스트)
- **2-tier 설정 시스템** — TOML → C++ 헤더 코드젠 (컴파일타임 상수 + 런타임 환경변수 오버라이드)
- **Result\<T, E\> 에러 처리** — C++23 `std::expected` 스타일 타입 안전 에러 핸들링

## Project Structure

```
network/          IOCP 비동기 I/O 정적 라이브러리
  inc/              acceptor/ client/ io/ socket/ server/ config/ transport/ util/
  src/
lib/              공용 유틸리티 정적 라이브러리
  inc/              memory/ logger/ error/ functional/ concurrency/ config/ macro/ scope/
  src/
protocol/         FlatBuffers 스키마 및 생성 코드
exec/
  echo/             에코 서버/클라이언트 (통합 테스트)
  chat/             채팅 서버/클라이언트 (FlatBuffers)
  playground/       실험/스크래치 코드
scripts/          TOML → C++ 코드젠 스크립트
docs/             아키텍처 문서
```

## Build

```powershell
# Visual Studio 2022, C++20
msbuild highp-mmorpg.slnx /p:Configuration=Release /p:Platform=x64
```

## Docs

자세한 문서는 [`docs/`](docs/INDEX.md) 참고

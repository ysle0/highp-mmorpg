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

## Highlights

- **IOCP async I/O** — `AcceptEx` 파이프라이닝, `GetQueuedCompletionStatus` 워커 풀, `std::jthread` + `stop_token` 기반 graceful shutdown
- **Memory pool 전략 4종** — Mutex 기반 / Thread-local / TLS-first Hybrid / Vyukov MPMC Lock-free, 벤치마크 포함
- **FlatBuffers 프로토콜** — 스키마 기반 zero-copy 직렬화, 채팅 프로토콜 구현 (로비/방/브로드캐스트)
- **2-tier 설정 시스템** — TOML → C++ 헤더 코드젠 (컴파일타임 상수 + 런타임 환경변수 오버라이드)
- **Result\<T, E\> 에러 처리** — C++23 `std::expected` 스타일 타입 안전 에러 핸들링

## Project Structure

```
network/          IOCP 비동기 I/O 정적 라이브러리
  inc/              acceptor/ client/ io/ socket/ server/ config/
  src/
lib/              공용 유틸리티 정적 라이브러리
  inc/              memory/ logger/ error/ functional/
  src/
protocol/         FlatBuffers 스키마 및 생성 코드
exec/
  echo/             에코 서버/클라이언트 (통합 테스트)
  chat/             채팅 서버/클라이언트 (FlatBuffers)
  benchmark/        메모리 풀 벤치마크
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

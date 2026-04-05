# Server Metrics Architecture

이 문서는 서버 metrics 모듈의 핵심 설계를 정리한다.

목표는 기존 서버 로직과 metrics 구현을 분리하면서도, 부하 테스트와 운영 분석에 필요한 지표를 안정적으로 수집하고 기록하는 것이다.

---

## Design Goals

- 기존 서버 로직과 metrics 구현 분리
- hot path 에서 file I/O 금지
- `metrics enabled = false`일 때 최소 오버헤드 유지
- 공용 서버 모듈로 재사용 가능하도록 설계
- run artifact 를 통해 결과 비교와 재현 가능성 확보

---

## Core Design

핵심 구조는 아래와 같다.

- 서버 코드는 metrics event 만 발행한다.
- `AtomicServerMetrics`가 메모리 안에서 값을 집계한다.
- 별도 `metrics writer thread`가 1초마다 snapshot 을 생성한다.
- snapshot 은 `server-metrics.jsonl`에 기록한다.
- 서버 종료 시 `summary.md` 와 `manifest.json`을 마무리한다.

즉, hot path 에서는 metrics 값만 갱신하고, 파일 기록은 전용 writer 가 맡는다.

---

## Execution Contexts

metrics 설계에서 가장 중요한 개념은 "어느 실행 컨텍스트가 어떤 값을 갱신하는가"이다.

### main thread

역할:

- metrics config 로드
- `run_id` 생성
- artifact 디렉터리 생성
- writer thread 시작 / 종료
- `manifest.json` 생성 및 종료 시각 반영

main thread 는 metrics lifecycle 을 관리하지만, 실시간 지표 값은 직접 갱신하지 않는다.

### IOCP worker threads

역할:

- `AcceptEx`, `WSARecv`, `WSASend` completion 처리
- connection / transport / IOCP 계열 이벤트 발행

주요 갱신 지표:

- `accept`
- `recv/send packets`
- `recv/send bytes`
- `pending accept/recv/send`
- `runnable worker thread count` 근사치
- `packet validation` 관련 이벤트
- queue push 이벤트

IOCP worker thread 는 hot path 이므로 파일 기록이나 무거운 계산을 하지 않는다.

### logic thread

역할:

- `PacketDispatcher::Tick()`
- packet handler 실행
- game logic 처리

주요 갱신 지표:

- `queue wait time`
- `dispatch process time`
- `tick duration`
- `tick lag`
- queue drain 이벤트

logic thread 역시 hot path 이므로 timing 측정 후 observer 호출만 수행한다.

### metrics writer thread

역할:

- 1초마다 metrics snapshot 생성
- `/sec` 값 계산
- process / logic CPU, thread count 수집
- `server-metrics.jsonl` 기록
- 종료 시 `summary.md` 생성

writer thread 는 유일하게 파일 I/O 를 수행하는 실행 컨텍스트다.

---

## Metrics Flow

전체 흐름은 아래와 같다.

1. 서버 코드가 이벤트를 발생시킨다.
2. 해당 실행 컨텍스트에서 `IServerMetrics` 메서드를 호출한다.
3. `AtomicServerMetrics`가 atomic counter, gauge, timing window 를 갱신한다.
4. `metrics writer thread`가 1초마다 현재 상태를 읽는다.
5. writer 가 snapshot 을 생성하고 `server-metrics.jsonl`에 append 한다.
6. 서버 종료 시 writer 가 누적 결과를 바탕으로 `summary.md` 를 생성한다.

핵심은 "이벤트 발생"과 "파일 기록"을 분리하는 것이다.

---

## Observer Pattern

서버 코드가 metrics 내부 구현을 몰라야 하므로 observer pattern 을 사용한다.

서버 코드가 의존하는 것은 `IServerMetrics` 인터페이스 뿐이다.

예시 이벤트:

- `OnAcceptRequestPosted()`
- `OnAcceptRequestCompleted()`
- `OnClientConnected()`
- `OnRecvRequestCompleted(size_t bytes)`
- `OnSendRequestCompleted(size_t bytes)`
- `OnPacketValidationSucceeded()`
- `OnPacketValidationFailed()`
- `OnQueuePushed()`
- `OnQueueDrained(size_t count)`
- `ObserveQueueWait(duration)`
- `ObserveDispatchProcess(duration)`
- `ObserveTickDuration(duration)`
- `ObserveTickLag(duration)`

이 구조 덕분에 metrics 를 끄면 `NoopServerMetrics`로 교체할 수 있고, 기존 서버 로직은 수정하지 않아도 된다.

---

## Why `network::Client` Can Participate

`network::Client`는 이름만 `Client`일 뿐, 서버 내부에서 연결 상태를 표현하는 객체다.

따라서 서버 metrics 관점에서 `network::Client`에 observer 를 연결하는 것은 자연스럽다.

이 방식의 장점:

- `pending recv/send`를 정확히 추적 가능
- `PostRecv`, `PostSend` 성공/실패를 가장 정확한 지점에서 계측 가능
- 나중에 실제 client app metrics 와 개념이 섞이지 않음

즉, `network::Client`는 서버 내부 connection representation 으로서 metrics observer 에 참여한다.

---

## Snapshot Strategy

snapshot 주기는 1초로 고정한다.

snapshot 에는 아래 종류의 값이 포함된다.

### Counter

누적 증가만 하는 값.

예:

- `accept_total`
- `disconnect_total`
- `recv_packets_total`
- `send_packets_total`
- `recv_bytes_total`
- `send_bytes_total`
- `packet_validation_failure_total`

snapshot 시점에는 이전 값과의 delta 로 `*_per_sec`를 계산한다.

### Gauge

현재 상태 값.

예:

- `connected_clients`
- `pending_accept_count`
- `pending_recv_count`
- `pending_send_count`
- `dispatcher_queue_length`
- `runnable_worker_thread_count`
- `thread_count`

### Timing Window

시간 계열 분포 요약.

예:

- `queue_wait_time`
- `dispatch_process_time`
- `tick_duration`
- `tick_lag`

v1 에서는 `count`, `sum`, `max`를 기록하고, snapshot 에서 `avg`와 `max`를 계산한다.

---

## Runtime Sampling

일부 지표는 이벤트 기반이 아니라 snapshot 시점에 OS 에서 읽어야 한다.

예:

- `process CPU percent`
- `logic thread CPU percent`
- `thread count`

이 값들은 `metrics writer thread`가 1초마다 조회한다.

이유:

- hot path 오버헤드 감소
- OS 조회 비용을 분리
- metrics 수집 책임을 writer 에 집중

---

## Output Files

v1 출력 파일은 아래 3개로 고정한다.

### `manifest.json`

run 메타데이터를 저장한다.

예:

- `run_id`
- `scenario_name`
- `started_at`
- `finished_at`
- `git_commit`
- `build_type`
- `server_name`
- `output_root`

### `server-metrics.jsonl`

1초마다 생성된 snapshot 원본을 append-only 로 저장한다.

이 파일은 나중에 그래프를 다시 그리거나, 다른 run 과 비교할 때 기준 데이터가 된다.

### `summary.md`

사람이 빠르게 읽을 수 있는 요약 결과를 저장한다.

핵심 지표만 남긴다.

예:

- max connected clients
- avg/max recv packets/sec
- avg/max send packets/sec
- avg/max queue wait
- avg/max tick duration
- avg/max tick lag
- max pending send count
- disconnect total
- packet validation failure total
- avg/max process CPU percent
- max thread count
- bottleneck guess

---

## Artifact Lifecycle

### server start

- metrics config 로드
- `run_id` 생성
- artifact 디렉터리 생성
- `manifest.json` 초기 작성
- writer thread 시작

### server running

- hot path 는 event 만 발행
- writer thread 가 1초마다 snapshot 기록

### server stop

- writer thread 종료
- 마지막 snapshot 반영
- `summary.md` 생성
- `manifest.json`에 `finished_at` 기록

---

## Metrics Disabled Mode

metrics 가 필요 없을 때는 `NoopServerMetrics`를 사용한다.

이 경우:

- 기존 서버 코드 변경 없음
- hot path 에서 observer 호출은 남아 있지만 실제 작업은 하지 않음
- 파일 생성 없음

즉, 기능을 완전히 제거하지 않고도 비용을 최소화할 수 있다.

---

## Design Summary

이 설계의 핵심은 아래 한 문장으로 정리할 수 있다.

> 서버의 각 실행 컨텍스트는 자신의 위치에서 metrics event 만 발행하고, `AtomicServerMetrics`가 이를 메모리 내에서 집계하며, 별도의 `metrics writer thread`가 1초마다 snapshot 을 생성해 artifact 파일로 기록한다.

이 구조는 다음을 만족한다.

- 기존 로직과 metrics 구현 분리
- hot path 보호
- 재사용 가능한 공용 서버 모듈
- 재현 가능한 부하 테스트 결과 보존

# Load Test Scenarios And Phase 1 Plan

이 문서는 현재 구현된 서버 metrics 기준으로 부하/성능 테스트 시나리오를 정리하고, 1차 필수 구현 범위를 계획으로 묶는다.

목표는 아래 두 가지다.

- 서버 각 계층의 오류를 드러내는 시나리오를 체계적으로 정의한다.
- 사람이 수동으로 확인하지 않아도 반복 실행 가능한 1차 필수 테스트 체계를 만든다.

---

## Current Scope

현재 기준에서 신뢰할 수 있는 관측 대상은 서버 metrics 이다.

- 서버 metrics 는 `server-metrics.jsonl`, `summary.md`, `manifest.json` 형태로 남는다.
- 핵심 진단 흐름은 `Connection -> Transport -> IOCP -> Queue -> Logic -> Protocol -> Runtime` 순서다.
- `chat-server`는 metrics writer 를 연결해 artifact 를 남긴다.

주의할 점도 있다.

- 클라이언트 metrics 문서는 존재하지만, 현재 코드베이스에는 RTT/timeout 계열 client metrics 구현이 없다.
- 따라서 1차 계획은 서버 metrics 와 서버 로그를 중심으로 작성한다.
- 클라이언트는 우선 "부하 발생기" 역할로 보고, 정밀 RTT 분석은 후속 단계로 미룬다.

---

## Diagnosis Principle

지표는 항상 앞 계층부터 읽는다.

1. Connection
2. Transport
3. IOCP
4. Queue
5. Logic
6. Protocol
7. Runtime

예를 들어 `dispatcher_queue_length`가 커졌더라도, 그 전에 `pending_send_count`가 폭증했다면 Queue 병목으로 단정하면 안 된다.

---

## Metrics Coverage

현재 시나리오 판정에 직접 사용하는 서버 지표는 아래와 같다.

| Layer | Primary Metrics |
|---|---|
| Connection | `connected_clients`, `accept_per_sec`, `disconnect_per_sec` |
| Transport | `recv_packets_per_sec`, `send_packets_per_sec`, `recv_bytes_per_sec`, `send_bytes_per_sec`, `send_fail_total` |
| IOCP | `runnable_worker_thread_count`, `pending_accept_count`, `pending_recv_count`, `pending_send_count` |
| Queue | `dispatcher_queue_length`, `queue_wait_ms_avg`, `queue_wait_ms_max` |
| Logic | `dispatch_process_ms_avg`, `dispatch_process_ms_max`, `tick_duration_ms_avg`, `tick_duration_ms_max`, `tick_lag_ms_avg`, `tick_lag_ms_max`, `logic_thread_cpu_percent` |
| Protocol | `packet_validation_total`, `packet_validation_failure_total` |
| Runtime | `process_cpu_percent`, `thread_count` |

현재 공백도 명확히 남긴다.

- `accept_post_fail_total` 없음
- `accept_completion_fail_total` 없음
- `recv_post_fail_total` 없음
- `client_pool_full_total` 없음
- `dispatch_fail_total` 없음
- `unhandled_payload_total` 없음
- disconnect reason 별 분리 없음
- client RTT / timeout metrics 없음

이 공백은 1차 구현 계획에 반영한다.

---

## Scenario Catalog

아래 시나리오는 "현재 구현된 서버 metrics 로 판별 가능한가"를 기준으로 정리했다.

### Phase 1 Required Scenarios

1차 필수는 자동화 우선순위가 높은 시나리오다.

| ID | Scenario | Purpose | Load Pattern | Main Layers | Main Metrics |
|---|---|---|---|---|---|
| P1-01 | `baseline-idle` | 서버 기본 기준선 확보 | 1~5 clients, 3 min idle | Runtime, IOCP, Logic | `pending_accept_count`, `pending_recv_count`, `tick_duration_ms_avg`, `thread_count` |
| P1-02 | `step-connect` | accept 파이프라인과 동접 수용량 확인 | `10 -> 50 -> 100 -> 300 -> 500` ramp | Connection, IOCP | `accept_per_sec`, `connected_clients`, `disconnect_per_sec`, `pending_accept_count` |
| P1-03 | `steady-small-traffic` | 정상 처리량 기준선 확보 | all clients, small packet, `1 msg/sec` | Transport, Queue, Logic | `recv/send_packets_per_sec`, `queue_wait_ms_avg`, `tick_duration_ms_avg` |
| P1-04 | `microburst` | 순간 폭주 시 queue/logic 병목 확인 | synchronized burst send | Queue, Logic, IOCP | `dispatcher_queue_length`, `queue_wait_ms_max`, `tick_lag_ms_max`, `runnable_worker_thread_count` |
| P1-05 | `broadcast-fanout` | fan-out send backlog 확인 | few senders, many receivers | Transport, IOCP | `send_packets_per_sec`, `send_bytes_per_sec`, `pending_send_count`, `send_fail_total` |
| P1-06 | `reconnect-storm` | 접속 churn 대응 확인 | short connect/disconnect loop | Connection, Runtime | `accept_per_sec`, `disconnect_per_sec`, `connected_clients`, `thread_count` |
| P1-07 | `malformed-packet` | verifier 실패 방어 확인 | invalid FlatBuffer mix-in | Protocol, Connection | `packet_validation_failure_total`, `disconnect_total` |
| P1-08 | `soak-30m` | 장시간 열화/누수 확인 | fixed load, 30 min | Runtime, Logic, Connection | `process_cpu_percent`, `logic_thread_cpu_percent`, `thread_count`, `disconnect_total` |

### Extended Scenarios

아래는 2차 이후 확장 시나리오다. 1차 문서에는 같이 정의만 해두고 자동화는 후순위로 둔다.

| ID | Scenario | Purpose | Expected Signal |
|---|---|---|---|
| E-01 | `idle-hold-high-conn` | 고동접 유휴 상태 안정성 확인 | `connected_clients` 높음, `recv/send_packets_per_sec` 낮음, `pending_recv_count` 유지 |
| E-02 | `all-to-all-chat-flood` | 전체 송수신 포화 구간 확인 | `recv/send_packets_per_sec` 급증, `tick_duration_ms_max`, `process_cpu_percent` 상승 |
| E-03 | `join-leave-churn` | room/session 경로 churn 확인 | `dispatch_process_ms_max`, `tick_duration_ms_max`, `dispatcher_queue_length` 상승 |
| E-04 | `slow-reader` | 느린 수신자에 대한 send backlog 확인 | `pending_send_count` 누적, `send_packets_per_sec` 하락 |
| E-05 | `slow-sender` | fragment/frame assembly 압박 확인 | `connected_clients` 유지, `recv_packets_per_sec` 낮음, `pending_recv_count` 유지 |
| E-06 | `oversized-payload` | oversized frame 방어 확인 | `packet_validation_failure_total`, `disconnect_total` 증가 |
| E-07 | `unknown-payload` | handler 미구현 payload 처리 확인 | 로그상 unhandled payload, disconnect 증가 |
| E-08 | `max-clients-overflow` | client pool 초과 처리 확인 | `connected_clients` 정체, reject 로그 증가 |
| E-09 | `graceful-shutdown-under-load` | 종료 시 artifact 완결성 확인 | `manifest.finished_at`, `summary.md` 정상 생성 |
| E-10 | `worker-pressure` | worker runnable contention 확인 | `runnable_worker_thread_count` 고착, throughput 정체 |
| E-11 | `send-heavy-large-message` | large payload send 경로 확인 | `send_bytes_per_sec`, `pending_send_count` 상승 |
| E-12 | `recv-heavy-small-message` | packet rate 높은 recv 경로 확인 | `recv_packets_per_sec` 우세, `queue_wait_ms_max` 상승 |
| E-13 | `mixed-good-bad-traffic` | 정상/비정상 혼합 입력에서 안정성 확인 | `packet_validation_failure_total` 증가에도 정상 traffic 유지 |
| E-14 | `single-room-hotspot` | 특정 room hotspot 확인 | send 집중, logic cost 상승 |
| E-15 | `multi-room-distribution` | room 분산 시 처리 편차 확인 | fan-out 대비 `dispatch_process_ms_avg` 완화 여부 |

---

## Phase 1 Required Scenario Details

### P1-01 `baseline-idle`

목적:

- 아무 부하가 없을 때 기본 상태를 고정한다.
- 이후 모든 시나리오 비교의 기준선으로 사용한다.

조건:

- clients: `1~5`
- action: connect only
- duration: `180 sec`

정상 기대:

- `connected_clients` 안정
- `pending_accept_count`가 backlog 수준에서 유지
- `pending_recv_count`가 연결 수 수준으로 유지
- `tick_duration_ms_avg` 매우 낮음
- `thread_count` 변동 거의 없음

의심 신호:

- idle 인데 `disconnect_per_sec` 발생
- idle 인데 `tick_lag_ms_max` 튐
- idle 인데 `thread_count`가 계속 증가

### P1-02 `step-connect`

목적:

- accept pipeline 유지 여부와 동접 증가 시 수용량을 본다.

조건:

- step: `10 -> 50 -> 100 -> 300 -> 500`
- each hold: `30~60 sec`
- send traffic: 없음 또는 최소

정상 기대:

- step 상승에 따라 `connected_clients` 증가
- `accept_per_sec`는 ramp 구간에서만 상승
- `pending_accept_count`는 급감 없이 재보충

의심 신호:

- 접속 시도 대비 `connected_clients`가 덜 오름
- `disconnect_per_sec`가 step 증가 직후 같이 급증
- `pending_accept_count`가 장시간 낮게 유지

### P1-03 `steady-small-traffic`

목적:

- 기본 처리량 기준선을 만든다.

조건:

- all clients joined
- small payload
- `1 msg/sec`
- duration: `300 sec`

정상 기대:

- `recv_packets_per_sec`와 `send_packets_per_sec`가 안정적
- `queue_wait_ms_avg`와 `tick_duration_ms_avg`가 낮고 평탄

의심 신호:

- packet rate 는 낮은데 `queue_wait_ms_avg`가 누적
- `logic_thread_cpu_percent`가 비정상적으로 높음

### P1-04 `microburst`

목적:

- 순간적으로 몰리는 입력에서 Queue/Logic 병목을 드러낸다.

조건:

- all clients synchronize
- short burst send
- burst between idle windows

정상 기대:

- burst 직후 `dispatcher_queue_length` 일시 상승 후 빠르게 회복
- `tick_lag_ms_max`는 튈 수 있으나 지속되지 않음

의심 신호:

- burst 이후에도 `dispatcher_queue_length` 회복 안 됨
- `queue_wait_ms_max`와 `tick_lag_ms_max`가 연속 증가

### P1-05 `broadcast-fanout`

목적:

- 적은 송신자가 많은 수신자에게 브로드캐스트할 때 send path 정체를 본다.

조건:

- few active senders
- many passive receivers
- medium or large message

정상 기대:

- `send_packets_per_sec` 증가
- `pending_send_count` 일시 상승 후 회복

의심 신호:

- `pending_send_count`가 계속 누적
- `send_fail_total` 증가
- `send_packets_per_sec`가 기대보다 낮게 고착

### P1-06 `reconnect-storm`

목적:

- 접속/종료 churn 이 connection lifecycle 과 runtime 안정성에 미치는 영향을 본다.

조건:

- connect, short hold, disconnect 반복
- many waves

정상 기대:

- `accept_per_sec`, `disconnect_per_sec`는 올라가도 `thread_count` 안정
- 종료 후 `connected_clients` 원복

의심 신호:

- churn 후 `connected_clients` 누수
- `thread_count` 증가가 회복되지 않음

### P1-07 `malformed-packet`

목적:

- verifier 실패와 프로토콜 이상 입력 방어를 검증한다.

조건:

- 일부 client 만 invalid FlatBuffer 전송
- 나머지는 정상 traffic 유지

정상 기대:

- malformed sender 만 disconnect
- `packet_validation_failure_total` 증가
- 전체 throughput 급락 없음

의심 신호:

- 소수 비정상 입력인데 `dispatcher_queue_length` 또는 `tick_duration_ms_max`가 크게 증가
- 정상 client 도 광범위하게 disconnect

### P1-08 `soak-30m`

목적:

- 누수, 열화, 장시간 drift 를 확인한다.

조건:

- fixed traffic
- duration: `1800 sec`

정상 기대:

- `thread_count` 안정
- `process_cpu_percent`와 `logic_thread_cpu_percent` 큰 추세 상승 없음
- `disconnect_total` 예외적으로 증가하지 않음

의심 신호:

- 시간 경과에 따라 `tick_duration_ms_avg` 지속 상승
- `thread_count` 계단식 증가
- 같은 부하인데 late stage 에 `disconnect_total` 증가

---

## Phase 1 Implementation Goal

1차 목표는 아래 한 문장으로 정리한다.

> 필수 8개 시나리오를 사람 입력 없이 반복 실행하고, 각 run 에 대해 서버 계층별 이상 여부를 `manifest.json`, `server-metrics.jsonl`, `summary.md`, client 결과 요약으로 남길 수 있는 상태를 만든다.

---

## Phase 1 Deliverables

1차 완료 시 반드시 있어야 하는 산출물은 아래와 같다.

- 비대화형 `chat-load-client`
- 시나리오 정의 파일 포맷
- 필수 8개 시나리오 템플릿
- `run_load_scenario.ps1`
- `run_required_suite.ps1`
- `compare_load_runs.ps1`
- 서버 manifest metadata 자동 채움
- 누락된 필수 failure counter 일부 보강
- 계획/실행/판정 기준 문서

---

## Work Breakdown

### 1. Load Client

목표:

- 현재 수동 `chat-client`를 대체할 비대화형 부하 발생기를 만든다.

필수 기능:

- multi-session connect
- join once
- periodic send
- burst send
- leave
- disconnect
- reconnect loop
- malformed packet send
- oversized payload send
- no-read 또는 slow-read mode

최소 출력:

- `connect_attempt_total`
- `connect_success_total`
- `connect_fail_total`
- `send_attempt_total`
- `send_success_total`
- `send_fail_total`
- `recv_total`
- `disconnect_total`

완료 조건:

- 사람 입력 없이 시나리오 파일만으로 실행 가능

### 2. Scenario Config

목표:

- 시나리오를 코드가 아니라 파일로 정의한다.

권장 필드:

- `scenario_name`
- `target_host`
- `target_port`
- `planned_clients`
- `duration_sec`
- `message_size_bytes`
- `send_interval_ms`
- `ramp_pattern`
- `action_mix`
- `malformed_ratio`
- `oversized_ratio`
- `read_mode`

완료 조건:

- 실행기 수정 없이 시나리오 추가 가능

### 3. Run Metadata Wiring

목표:

- 서버와 부하 발생기가 같은 `run_id`와 시나리오 메타데이터를 공유한다.

서버 manifest 에 채울 필드:

- `scenario_name`
- `git_commit`
- `git_branch`
- `target_host`
- `planned_clients`
- `message_size_bytes`
- `send_interval_ms`
- `duration_sec`

완료 조건:

- 나중에 artifact 만 보고도 테스트 조건을 복원 가능

### 4. Metrics Gap Fill

1차에서 최소한 추가할 항목:

- `accept_post_fail_total`
- `accept_completion_fail_total`
- `recv_post_fail_total`
- `client_pool_full_total`
- `dispatch_fail_total`
- `unhandled_payload_total`

추가 권장:

- disconnect reason 별 counter

완료 조건:

- "어느 계층에서 실패했는가"를 metrics 만으로 1차 분류 가능

### 5. Scenario Runner Scripts

필수 스크립트:

- `run_load_scenario.ps1`
- `run_required_suite.ps1`
- `compare_load_runs.ps1`

기능:

- server start
- `run_id` 생성
- 환경변수 주입
- load client 실행
- 종료 후 artifact 정리
- 반복 3회 실행
- 결과 비교표 생성

완료 조건:

- 명령 1개로 1차 필수 suite 실행 가능

### 6. Documentation

필수 문서:

- 이 문서
- 시나리오 파일 예시
- 실행 방법
- 판정 기준

완료 조건:

- 새로운 기여자가 문서만 보고 suite 를 실행 가능

---

## Recommended Execution Order

1. `baseline-idle`
2. `step-connect`
3. `steady-small-traffic`
4. `microburst`
5. `broadcast-fanout`
6. `reconnect-storm`
7. `malformed-packet`
8. `soak-30m`

이 순서를 권장하는 이유는 아래와 같다.

- 먼저 기준선을 확보해야 뒤 시나리오 해석이 쉬워진다.
- 그 다음 accept/connection 경로를 확인해야 transport 해석이 왜곡되지 않는다.
- 그 다음 steady load 와 burst 로 queue/logic 병목을 분리한다.
- 마지막에 churn, bad input, long soak 를 통해 운영형 문제를 확인한다.

---

## Acceptance Criteria

1차 구현 완료 조건은 아래와 같다.

- 필수 8개 시나리오가 자동 실행된다.
- 각 run 마다 `manifest.json`, `server-metrics.jsonl`, `summary.md`가 남는다.
- 서버 manifest 에 시나리오 메타데이터가 채워진다.
- 필수 failure counter 가 snapshot 과 summary 에 반영된다.
- 같은 시나리오를 3회 반복 실행해 비교 가능하다.
- `summary.md`만 보고도 병목 계층을 1차 분류할 수 있다.

---

## Out Of Scope For Phase 1

아래는 중요하지만 1차 필수에는 넣지 않는다.

- client RTT / p95 / timeout metrics 정식 구현
- 시각화 대시보드
- distributed multi-host load generation
- 자동 임계치 기반 pass/fail 판정
- room 별 상세 business metrics

---

## Suggested File Layout

권장 파일 배치는 아래와 같다.

```text
docs/
  LOAD_TEST_SCENARIOS_AND_PLAN.md

scripts/
  run_load_scenario.ps1
  run_required_suite.ps1
  compare_load_runs.ps1

exec/chat/
  chat-load-client/

artifacts/load-tests/<run-id>/
  manifest.json
  summary.md
  server-metrics.jsonl
  client-results.json
  server.log
  client.log
```

---

## Notes

- 현재 문서는 "지금 구현된 metrics" 기준으로 작성했다.
- client metrics 구현이 들어오면 `steady-small-traffic`, `microburst`, `soak-30m` 시나리오에 RTT 기반 판정 항목을 추가한다.
- `max-clients-overflow`, `unknown-payload`, `slow-reader`는 실제 운영 문제와 가깝기 때문에 2차 우선순위가 높다.

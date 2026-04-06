# Performance Test Scenarios

이 문서는 현재 코드베이스에서 바로 의미 있게 사용할 수 있는 클라이언트-서버 성능 테스트 시나리오를 정리한다.

대상은 기본적으로 [chat-server](/Users/ysl/werk/highp-mmorpg/exec/chat/chat-server/chat-server/main.cpp)다. 현재 `server/metrics` writer는 chat server에만 연결되어 있고, echo server는 아직 같은 수준의 server metrics artifact를 남기지 않는다.

---

## Scope

- 서버 기준 관측값: `server-metrics.jsonl`, `summary.md`, `manifest.json`
- 핵심 병목 축: `accept -> recv/send -> dispatcher queue -> logic tick -> room fan-out`
- 현재 지원 workload: `JoinRoomRequest`, `LeaveRoomRequest`, `SendMessageRequest`
- 현재 room 모델: `max_capacity = 100`, 초과 시 새 room 자동 생성

즉, 지금 단계의 대표 시나리오는 `접속`, `입장`, `채팅 fan-out`, `퇴장`, `장시간 안정성`을 중심으로 잡는 것이 맞다.

---

## Current Constraints

- 현재 [chat client](/Users/ysl/werk/highp-mmorpg/exec/chat/chat-client/main.cpp)는 수동 CLI 중심이라 대규모 부하 생성기로는 부족하다.
- client metrics는 아직 구현보다 문서 정의가 앞서 있다. 따라서 이번 시나리오의 1차 판단 기준은 서버 metrics다.
- 시나리오 이름과 실행 조건은 [Load Test Reproducibility Guide](/Users/ysl/werk/highp-mmorpg/docs/LOAD_TEST_REPRODUCIBILITY.md)의 `scenario_name`, `planned_clients`, `message_size_bytes`, `send_interval_ms`, `duration_sec` 형식에 맞춰 기록하는 것을 전제로 한다.

---

## Recommended Order

| # | Scenario | 목적 |
|---|---|---|
| 1 | Metrics Smoke | metrics 파이프라인 자체 확인 |
| 2 | Steady Single Room | 가장 일반적인 정상 부하 baseline |
| 3 | Step Load Ramp | 안전한 운영 구간 파악 |
| 4 | Connection/Join Storm | 접속 폭주와 join fan-out 확인 |
| 5 | Traffic Burst Spike | 짧은 순간 폭증 후 회복력 확인 |
| 6 | Single-Room Fan-out Stress | 현재 구조의 최악 fan-out 경로 측정 |
| 7 | Multi-Room Distributed Load | room 분산 효과 확인 |
| 8 | Reconnect Churn | 세션 lifecycle 흔들림 확인 |
| 9 | Long Soak | 누수, drift, 장시간 안정성 확인 |

---

## 1. Metrics Smoke

가장 먼저 돌릴 시나리오다. 목표는 throughput이 아니라 `metrics가 정상적으로 남는가`를 확인하는 것이다.

- `scenario_name`: `chat-metrics-smoke`
- 시작 조건:
  `planned_clients=1~5`, `message_size_bytes=32~128`, `send_interval_ms=3000~5000`, `duration_sec=60~120`
- 부하 형태:
  클라이언트가 순차적으로 접속하고 방에 입장한 뒤, 아주 느린 속도로 메시지를 보낸다.
- 핵심 metrics:
  `connected_clients`, `accept_per_sec`, `recv_packets_per_sec`, `send_packets_per_sec`, `packet_validation_failure_total`
- 기대 신호:
  `manifest.json`, `server-metrics.jsonl`, `summary.md`가 모두 생성되고, `dispatcher_queue_length`, `tick_lag_ms_max`, `send_fail_total`이 거의 0에 머문다.
- 실패 신호:
  metrics 파일 누락, `packet_validation_failure_total > 0`, 소수 클라이언트인데도 `queue_wait_ms_max`와 `tick_duration_ms_max`가 튄다.

---

## 2. Steady Single Room

가장 보편적인 정상 사용 시나리오다. 한 방에 적당한 수의 유저가 계속 채팅하는 형태다.

- `scenario_name`: `chat-steady-single-room`
- 시작 조건:
  `planned_clients=20~100`, `message_size_bytes=64~256`, `send_interval_ms=1000`, `duration_sec=300`
- 부하 형태:
  모든 클라이언트가 하나의 room에 들어가고, 각 클라이언트가 1초에 1회 메시지를 보낸다.
- 핵심 metrics:
  `recv_packets_per_sec`, `send_packets_per_sec`, `send_bytes_per_sec`, `dispatcher_queue_length`, `queue_wait_ms_avg`, `tick_duration_ms_avg`, `logic_thread_cpu_percent`
- 기대 신호:
  `send_packets_per_sec`는 room fan-out 때문에 `recv_packets_per_sec`보다 훨씬 높게 나오지만, queue/tick 지표는 안정적으로 유지된다.
- 실패 신호:
  steady load인데 `dispatcher_queue_length`가 계속 누적되거나 `tick_lag_ms_avg`가 지속적으로 증가한다.

---

## 3. Step Load Ramp

안전한 운영 구간과 꺾이는 지점을 찾는 시나리오다. 가장 실용적인 capacity planning 시작점이다.

- `scenario_name`: `chat-step-load-ramp`
- 시작 조건:
  `10 -> 30 -> 50 -> 75 -> 100 clients`, 각 단계 `duration_sec=180~300`, `message_size_bytes=128`, `send_interval_ms=1000`
- 부하 형태:
  동일한 workload를 유지한 채 동접만 단계적으로 올린다.
- 핵심 metrics:
  `connected_clients`, `recv_packets_per_sec`, `send_packets_per_sec`, `dispatcher_queue_length`, `queue_wait_ms_avg`, `tick_duration_ms_avg`, `tick_lag_ms_avg`, `process_cpu_percent`, `logic_thread_cpu_percent`
- 기대 신호:
  낮은 단계에서는 거의 선형적으로 증가하다가, 특정 구간부터 queue/tick/cpu 지표가 급격히 꺾인다.
- 실패 신호:
  낮은 단계부터 `pending_send_count`와 `dispatcher_queue_length`가 빠르게 증가하거나, 단계가 바뀌어도 회복하지 못한다.

---

## 4. Connection/Join Storm

로그인 피크나 오픈 직후처럼 짧은 시간 안에 접속과 방 입장이 몰리는 상황을 본다.

- `scenario_name`: `chat-connection-join-storm`
- 시작 조건:
  `planned_clients=50~200`, `message_size_bytes=32~64`, `send_interval_ms=0`, `duration_sec=60~120`
- 부하 형태:
  짧은 시간 안에 클라이언트를 한꺼번에 연결시키고 즉시 join 시킨다. steady chat은 거의 하지 않는다.
- 핵심 metrics:
  `accept_per_sec`, `connected_clients`, `pending_accept_count`, `recv_packets_per_sec`, `send_packets_per_sec`, `runnable_worker_thread_count`, `dispatcher_queue_length`
- 기대 신호:
  접속/입장 순간 피크는 크지만 짧고, 피크가 지난 뒤 queue와 pending 계열이 빠르게 원상복구된다.
- 실패 신호:
  `pending_accept_count`가 오래 유지되거나, join 이후 `dispatcher_queue_length`와 `tick_lag_ms_max`가 길게 남는다.

---

## 5. Traffic Burst Spike

평소에는 조용하다가 짧은 시간 동안 메시지가 몰리는 형태다. 채팅 서버에서 흔한 실전 패턴이다.

- `scenario_name`: `chat-burst-spike`
- 시작 조건:
  `planned_clients=50~100`, `message_size_bytes=128~512`, 평시 `send_interval_ms=5000`, burst 구간 `send_interval_ms=100~200`, `duration_sec=180~300`
- 부하 형태:
  대부분 시간은 저부하로 유지하다가 5~15초 정도 모든 클라이언트가 동시에 빠르게 메시지를 보낸다.
- 핵심 metrics:
  `recv_packets_per_sec`, `send_packets_per_sec`, `pending_send_count`, `dispatcher_queue_length`, `queue_wait_ms_max`, `tick_lag_ms_max`
- 기대 신호:
  burst 구간에서 순간 피크는 생기더라도, burst가 끝난 뒤 queue/tick 지표가 빠르게 정상 범위로 복귀한다.
- 실패 신호:
  짧은 burst 이후에도 `pending_send_count`나 `dispatcher_queue_length`가 계속 남아있고, `send_fail_total`이 증가한다.

---

## 6. Single-Room Fan-out Stress

현재 chat server 구조에서 가장 직접적인 최악 경로다. 한 방에 가득 찬 유저들에게 브로드캐스트를 반복시킨다.

- `scenario_name`: `chat-single-room-fanout-stress`
- 시작 조건:
  `planned_clients=80~100`, `message_size_bytes=128~512`, `send_interval_ms=100~300`, `duration_sec=180~300`
- 부하 형태:
  모든 클라이언트를 하나의 room에 몰아넣고, 한 명 또는 소수의 hot sender가 지속적으로 메시지를 보낸다.
- 핵심 metrics:
  `send_packets_per_sec`, `send_bytes_per_sec`, `pending_send_count`, `dispatcher_queue_length`, `dispatch_process_ms_avg`, `tick_duration_ms_avg`, `logic_thread_cpu_percent`
- 기대 신호:
  recv보다 send가 훨씬 가파르게 커지지만, logic tick과 send backlog가 통제 가능 범위 안에 머문다.
- 실패 신호:
  `pending_send_count`가 지속적으로 증가하고 `dispatch_process_ms_max`, `tick_duration_ms_max`, `tick_lag_ms_max`가 함께 치솟는다.

---

## 7. Multi-Room Distributed Load

총 동접 수는 유지하되 room을 여러 개로 분산시켜, 병목이 전체 세션 수 때문인지 room fan-out 때문인지 구분하는 시나리오다.

- `scenario_name`: `chat-multi-room-distributed`
- 시작 조건:
  `planned_clients=200~400`, `message_size_bytes=128`, `send_interval_ms=500~1000`, `duration_sec=300`
- 부하 형태:
  클라이언트를 여러 room으로 나누고 각 room 내부에서만 채팅하게 한다. room당 인원은 `max_capacity=100` 이하로 유지한다.
- 핵심 metrics:
  `connected_clients`, `recv_packets_per_sec`, `send_packets_per_sec`, `send_bytes_per_sec`, `dispatcher_queue_length`, `tick_duration_ms_avg`
- 기대 신호:
  총 동접이 늘어도 room fan-out이 분산되면 single-room stress보다 `pending_send_count`와 `tick` 계열이 완만해진다.
- 실패 신호:
  room을 나눴는데도 single-room stress와 유사한 `queue/tick` 악화가 나타나면 fan-out 외 다른 공용 병목을 의심해야 한다.

---

## 8. Reconnect Churn

실사용에서는 steady client만 있는 것이 아니라, 접속과 종료가 계속 섞인다. 이 시나리오는 세션 lifecycle 비용과 흔들림을 본다.

- `scenario_name`: `chat-reconnect-churn`
- 시작 조건:
  `planned_clients=50~150`, `message_size_bytes=64~128`, `send_interval_ms=1000~2000`, `duration_sec=300~600`
- 부하 형태:
  전체 클라이언트의 일부가 주기적으로 disconnect 후 reconnect 하고 다시 join 한다.
- 핵심 metrics:
  `accept_per_sec`, `disconnect_per_sec`, `connected_clients`, `pending_accept_count`, `dispatcher_queue_length`, `thread_count`
- 기대 신호:
  connect/disconnect 피크는 생기더라도 `connected_clients`가 목표 범위로 복귀하고 `thread_count`가 비정상적으로 증가하지 않는다.
- 실패 신호:
  churn이 반복될수록 `thread_count`가 올라가거나, reconnect 이후 `pending_accept_count`와 `queue_wait_ms_avg`가 회복하지 못한다.

---

## 9. Long Soak

단기 burst보다 더 중요한 경우가 많다. 낮거나 중간 수준의 부하를 오래 유지해 resource leak과 drift를 본다.

- `scenario_name`: `chat-long-soak`
- 시작 조건:
  `planned_clients=30~100`, `message_size_bytes=64~256`, `send_interval_ms=1000~3000`, `duration_sec=1800~7200`
- 부하 형태:
  중간 정도의 steady load를 30분 이상, 가능하면 1~2시간 이상 유지한다.
- 핵심 metrics:
  `process_cpu_percent`, `logic_thread_cpu_percent`, `thread_count`, `disconnect_total`, `dispatcher_queue_length`, `queue_wait_ms_avg`, `tick_lag_ms_avg`
- 기대 신호:
  throughput은 큰 변화 없이 유지되고, CPU/thread/queue 지표에 장시간 우상향 drift가 없다.
- 실패 신호:
  시간이 지날수록 `thread_count`, `queue_wait_ms_avg`, `tick_duration_ms_avg`가 조금씩 상승하거나 설명되지 않는 disconnect가 누적된다.

---

## Reading Rules

시나리오마다 가장 먼저 읽을 순서는 동일하다.

1. `connected_clients`, `accept_per_sec`, `disconnect_per_sec`
2. `recv_packets_per_sec`, `send_packets_per_sec`, `recv_bytes_per_sec`, `send_bytes_per_sec`
3. `pending_accept_count`, `pending_recv_count`, `pending_send_count`, `runnable_worker_thread_count`
4. `dispatcher_queue_length`, `queue_wait_ms_avg`, `queue_wait_ms_max`
5. `dispatch_process_ms_avg`, `tick_duration_ms_avg`, `tick_lag_ms_avg`
6. `process_cpu_percent`, `logic_thread_cpu_percent`, `thread_count`

앞 단계가 무너지면 뒤 단계 해석이 왜곡된다.

---

## Minimal First Batch

처음 자동화할 때는 아래 4개부터 시작하는 것이 가장 효율적이다.

- `chat-metrics-smoke`
- `chat-steady-single-room`
- `chat-step-load-ramp`
- `chat-single-room-fanout-stress`

이 4개만 있어도 `metrics 수집 확인`, `정상 baseline`, `한계점 탐색`, `최악 fan-out`까지 커버된다.

---

## Next Step

다음 구현 단계는 수동 [chat client](/Users/ysl/werk/highp-mmorpg/exec/chat/chat-client/main.cpp)를 대체할 load generator를 만드는 것이다.

그 load generator는 최소한 아래를 제어할 수 있어야 한다.

- 동시 접속 수
- join/leave 시점
- room 분배 방식
- 메시지 크기
- 메시지 전송 주기
- burst/step/churn 스케줄
- run별 `scenario_name`과 manifest 메타데이터 기록

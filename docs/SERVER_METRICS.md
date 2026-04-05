**Server Metrics Guide**

부하 테스트와 운영 중 장애 분석 시 확인할 서버 지표 정리.

이 문서는 지표를 많이 나열하는 대신, 실제로 어떤 순서로 읽고 어떤 이상을 의심할지에 초점을 둔다.

---

**읽는 순서**

`Connection -> Transport -> IOCP -> Queue -> Logic -> Protocol -> Runtime`

앞 단계에 이상이 있으면 뒤 단계 지표 해석이 왜곡될 수 있으므로 항상 앞 단계부터 확인.

---

**빠른 진단 흐름**

| Step | Group | 먼저 확인할 질문 |
|---|---|---|
| 1 | Connection | 접속 수와 접속/종료 흐름이 정상인가 |
| 2 | Transport | 송수신 처리량이 기대한 수준인가 |
| 3 | IOCP | worker와 pending I/O 상태가 정상인가 |
| 4 | Queue | logic 앞단 큐가 밀리고 있는가 |
| 5 | Logic | tick 또는 handler가 병목인가 |
| 6 | Protocol | 잘못된 패킷이나 프로토콜 불일치가 있는가 |
| 7 | Runtime | 프로세스 전체 자원 상태가 비정상인가 |

---

**1. Connection**

목적: 현재 접속 상태와 접속/이탈 흐름 확인.

| Metric | 의미 | 이상 시 확인 |
|---|---|---|
| `connected clients` | 현재 살아있는 TCP 연결 수 | 동접 규모 급증, 연결 누수, 예상보다 낮은 수용량 |
| `accept/sec` | 초당 접속 성공 수 | 접속 폭주, 로그인 피크, 접속 패턴 급변 |
| `disconnect/sec` | 초당 연결 종료 수 | 이탈 폭주, 비정상 종료 증가, 연결 유지 실패 |

이 그룹에서 이상이 보이면 접속 폭주, 이탈 폭주, 연결 생명주기 이상 먼저 확인.

---

**2. Transport**

목적: 네트워크 데이터 처리량 확인.

| Metric | 의미 | 이상 시 확인 |
|---|---|---|
| `recv packets/sec` | 서버가 받아들인 패킷 처리량 | 입력 트래픽 변화, 패킷 처리 감소 |
| `send packets/sec` | 실제 송신 완료된 패킷 처리량 | 응답량 감소, 송신 정체 |
| `recv bytes/sec` | 초당 수신 바이트 수 | 큰 메시지 유입, 트래픽 규모 변화 |
| `send bytes/sec` | 초당 송신 바이트 수 | 출력 부하 증가, 브로드캐스트 규모 변화 |
| `send fail count` | `PostSend` 실패 수 | 송신 경합, 소켓 상태 이상, 송신 경로 정체 |

이 그룹에서 이상이 보이면 네트워크 부하 증가, 송신 정체, 소켓 계층 문제 확인.

---

**3. IOCP**

목적: completion worker와 pending I/O 상태 확인.

| Metric | 의미 | 이상 시 확인 |
|---|---|---|
| `runnable worker thread count` | 실제 runnable 또는 busy 상태에서 completion을 처리 중인 IOCP worker 수 | worker starvation, runnable contention, scheduler pressure |
| `pending accept count` | 완료 대기 중인 `AcceptEx` 요청 수 | accept 파이프라인 유지 실패, accept 재발행 누락 |
| `pending recv count` | 완료 대기 중인 `recv` 요청 수 | recv 재발행 누락, 연결 수 대비 비정상적으로 낮은 pending 상태 |
| `pending send count` | 완료 대기 중인 `send` 요청 수 | 송신 backlog, 느린 클라이언트, send completion 지연 |

주의: `runnable worker thread count`는 Async I/O 요청 개수가 아니라 현재 실제로 바쁜 worker 수다.

이 그룹에서 이상이 보이면 worker starvation, runnable contention, accept 파이프라인 이상, completion 처리 지연 확인.

---

**4. Queue**

목적: IOCP에서 Logic으로 넘어가는 중간 구간의 적체 확인.

| Metric | 의미 | 이상 시 확인 |
|---|---|---|
| `dispatcher queue length` | logic thread가 처리하기 위해 대기 중인 패킷 수 | logic이 입력을 따라가지 못하는 상태 |
| `queue wait time (ms)` | `recv -> dispatcher -> logic thread`까지 도달하는 대기 시간 | logic 앞단 대기 증가, 백프레셔 누적 |

이 그룹에서 이상이 보이면 Logic 그룹을 집중 확인.

---

**5. Logic**

목적: tick 및 handler 처리 비용 확인.

| Metric | 의미 | 이상 시 확인 |
|---|---|---|
| `tick duration (ms)` | Tick 1회 처리 시간 | tick 처리 자체 과부하 |
| `tick lag (ms)` | 목표 tick 시점 대비 실제 tick 밀림 정도 | tick 병목, 누적 지연 |
| `dispatch process time (ms)` | logic thread에서 각 handler 처리 시간 | 특정 handler 비용 과다 |
| `logic thread CPU percent` | logic thread CPU 사용률 | logic thread 자체 병목 |

`dispatcher queue length`와 `tick duration`이 함께 증가하면 logic 병목 가능성이 높다.

---

**6. Protocol**

목적: 잘못된 입력과 프로토콜 불일치 확인.

| Metric | 의미 | 이상 시 확인 |
|---|---|---|
| `packet validation total count` | 패킷 검증 총 횟수 | 전체 검증 규모 |
| `packet validation failure count` | 패킷 검증 실패 횟수 | 악성 패킷, 손상 데이터, 프로토콜 불일치 |
| `packet validation failure ratio (%)` | 전체 검증 대비 실패 비율 | 실패 비중 증가 여부 |

이 그룹에서 이상이 보이면 악성 패킷, 손상 데이터, 프로토콜 버전 불일치, 잘못된 직렬화 데이터 확인.

---

**7. Runtime**

목적: 서버 프로세스 전체 건강 상태 확인.

| Metric | 의미 | 이상 시 확인 |
|---|---|---|
| `process CPU percent` | 서버 프로세스 전체 CPU 사용률 | 전체 자원 압박, 구조적 병목 |
| `thread count` | 현재 스레드 수 | 의도하지 않은 스레드 증가, 누수, 잘못된 설정 |

이 그룹에서 이상이 보이면 프로세스 전체 자원 이상, 설정 문제, 구조적 런타임 문제 확인.

# Load Test Reproducibility Guide

이 문서는 부하 테스트 결과를 "재현 가능하다"라고 설명하기 위해 무엇을 기록하고 어떻게 비교해야 하는지 정리한다.

재현 가능성의 핵심은 단순히 테스트를 한 번 성공하는 것이 아니라, 같은 조건에서 여러 번 실행했을 때 비슷한 결과가 반복되는 것을 보여주는 데 있다.

---

## What Reproducibility Means

아래 네 가지가 같을 때 결과가 비슷하게 반복되어야 한다.

- 같은 코드
- 같은 설정
- 같은 시나리오
- 같은 환경

즉, "같은 조건을 고정해서 다시 돌렸을 때, 주요 지표가 허용 범위 안에서 반복된다"를 보여주는 것이 목적이다.

---

## What Must Be Fixed

부하 테스트 결과를 비교하려면 아래 항목을 반드시 고정하거나 명시해야 한다.

- git commit SHA
- branch
- build type (`Debug` / `Release`)
- 서버 설정 파일
- 클라이언트 설정 파일
- 테스트 시나리오 이름
- 동접 수
- 메시지 크기
- 메시지 전송 주기
- 테스트 duration
- 서버 머신 정보
- 클라이언트 머신 정보
- 실행 시각

이 정보가 없으면 결과는 남아 있어도 나중에 비교하기 어렵다.

---

## Recommended Run Artifacts

부하 테스트 1회 실행을 하나의 run으로 저장한다.

권장 폴더 구조:

```text
artifacts/load-tests/<run-id>/
  manifest.json
  summary.md
  server-metrics.jsonl
  client-metrics.jsonl
  server.log
  client.log
  notes.md
```

예시 run id:

- `2026-04-05-echo-smoke-run1`
- `2026-04-05-chat-step-load-100c-run2`

---

## Manifest

`manifest.json`은 "어떤 조건에서 테스트를 돌렸는가"를 남기는 파일이다.

권장 필드:

- `run_id`
- `scenario_name`
- `started_at`
- `finished_at`
- `git_commit`
- `git_branch`
- `build_type`
- `server_config_path`
- `client_config_path`
- `target_host`
- `target_port`
- `planned_clients`
- `message_size_bytes`
- `send_interval_ms`
- `duration_sec`
- `server_machine`
- `client_machine`

예시:

```json
{
  "run_id": "2026-04-05-chat-step-load-100c-run1",
  "scenario_name": "chat-step-load",
  "started_at": "2026-04-05T21:10:00Z",
  "finished_at": "2026-04-05T21:15:00Z",
  "git_commit": "abc1234",
  "git_branch": "main",
  "build_type": "Release",
  "server_config_path": "exec/chat/chat-server/chat-server/config.runtime.toml",
  "client_config_path": "exec/chat/chat-client/config.runtime.toml",
  "target_host": "127.0.0.1",
  "target_port": 8080,
  "planned_clients": 100,
  "message_size_bytes": 128,
  "send_interval_ms": 1000,
  "duration_sec": 300,
  "server_machine": "i7-13700K / 32GB / Windows 11",
  "client_machine": "same-host"
}
```

---

## Summary

`summary.md`는 "이번 run에서 무엇이 일어났는가"를 빠르게 읽기 위한 요약 파일이다.

권장 항목:

- 테스트 목적
- 부하 조건
- 핵심 결과
- 이상 징후
- 병목 추정
- 이전 run 대비 차이

예시:

```md
목적
- 100 clients 고정 부하에서 chat server의 기본 안정성 확인

부하 조건
- 100 clients
- 128B packet
- 1 msg/sec
- 5 min

핵심 결과
- avg RTT: 12ms
- p95 RTT: 19ms
- max dispatcher queue length: 5
- avg tick duration: 2.1ms
- disconnect total: 0

병목 추정
- 뚜렷한 Queue/Logic 병목 없음
```

---

## Repeat Runs

재현 가능성을 보여주려면 같은 시나리오를 최소 3회 반복하는 것이 좋다.

예시:

- `chat-step-load-100c-run1`
- `chat-step-load-100c-run2`
- `chat-step-load-100c-run3`

비교할 핵심 항목:

- avg RTT
- p95 RTT
- max dispatcher queue length
- avg tick duration
- disconnect total

---

## Comparison Table

재현 가능성을 보여줄 때는 반복 실행 결과를 표로 남기는 것이 가장 좋다.

예시:

| Run | Avg RTT | P95 RTT | Max Queue Length | Avg Tick Duration | Disconnects |
|---|---:|---:|---:|---:|---:|
| run1 | 12ms | 19ms | 5 | 2.1ms | 0 |
| run2 | 11ms | 18ms | 4 | 2.0ms | 0 |
| run3 | 13ms | 20ms | 5 | 2.2ms | 0 |

이 표를 통해 "우연히 한 번 잘 나온 결과"가 아니라는 점을 보여줄 수 있다.

---

## Tolerance

재현 가능성은 결과가 완전히 동일하다는 뜻이 아니다.

중요한 것은 주요 지표가 허용 범위 안에서 반복되는지다.

예시:

- avg RTT: `12ms ± 2ms`
- p95 RTT: `20ms ± 3ms`
- avg tick duration: `2.1ms ± 0.3ms`

허용 범위를 함께 기록하면 비교가 더 명확해진다.

---

## Portfolio-Friendly Evidence

포트폴리오에서 재현 가능성을 보여줄 때는 아래 세 가지가 가장 효과적이다.

- `manifest.json`
- `summary.md`
- 반복 실행 비교 표

이 세 가지가 있으면 아래를 자연스럽게 설명할 수 있다.

- 어떤 조건에서 테스트했는가
- 어떤 결과가 나왔는가
- 같은 조건에서 비슷한 결과가 반복되었는가

---

## Recommended Claim Format

포트폴리오나 문서에 아래와 같이 작성하면 좋다.

> 동일한 커밋과 동일한 서버/클라이언트 설정으로 `100 clients, 5 min, 128B packet, 1 msg/sec` 시나리오를 3회 반복 실행했다.
> 평균 RTT는 `11~13ms`, p95 RTT는 `18~20ms`, 최대 dispatcher queue length는 `4~5` 범위로 수렴했다.
> 이를 통해 현재 측정 체계가 동일 조건에서 유사한 결과를 반복적으로 산출함을 확인했다.

---

## Minimal Checklist

아래 항목이 모두 있으면 "재현 가능한 부하 테스트 결과"라고 말할 수 있다.

- run별 manifest 저장
- run별 summary 저장
- 원본 metrics 파일 저장
- 같은 시나리오 3회 이상 반복
- 비교 표 작성
- 허용 오차 범위 명시


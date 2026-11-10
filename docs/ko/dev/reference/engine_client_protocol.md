---
title: 엔진/클라이언트 프로토콜 0.3
---

외부 클라이언트는 `cataclysm-bn-tiles --client=mcp`의 stdio에서 줄바꿈으로 구분된 JSON-RPC 2.0으로 게임을 플레이합니다. 엔진이 각 화면과 그 화면에서 할 수 있는 일을 설명하므로, 클라이언트에는 게임 규칙이 필요 없습니다.

## 세션

```text
-> {"jsonrpc":"2.0","id":1,"method":"bn.hello","params":{"versions":["0.3"],"client":{"name":"my-client","version":"1"}}}
<- {"jsonrpc":"2.0","id":1,"result":{"version":"0.3","epoch":"epoch:e7f3",...}}
-> {"jsonrpc":"2.0","id":2,"method":"bn.subscribe","params":{}}
<- {"jsonrpc":"2.0","id":2,"result":{"at":{"epoch":"epoch:e7f3","sequence":"40","revision":"31"},"interaction":{...},"entities":[],"parts":1}}
<- {"jsonrpc":"2.0","method":"bn.snapshot.part","params":{"epoch":"epoch:e7f3","at":{...},"index":0,"last":true,"cells":[...]}}
-> {"jsonrpc":"2.0","id":3,"method":"bn.command.submit","params":{"epoch":"epoch:e7f3","expect":{"revision":"31","boundary_id":"boundary:90"},"operation":{"kind":"action","action_id":"RIGHT"}}}
<- {"jsonrpc":"2.0","id":3,"result":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"received"}}
<- {"jsonrpc":"2.0","method":"bn.events","params":{"epoch":"epoch:e7f3","events":[{"sequence":"41","revision":"32","type":"interaction.changed","command":"c:5","changes":{...}}]}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"completed","at":{"epoch":"epoch:e7f3","sequence":"41","revision":"32"}}}
```

`parts`개의 스냅샷 파트를 모은 다음 `bn.events`를 순서대로 적용합니다. 순서 누락, epoch 불일치, `bn.resync`가 오면 `bn.subscribe`를 다시 호출하세요.

## 메서드

| 메서드                   | 파라미터                                                  | 결과                                                                           |
| ------------------------ | --------------------------------------------------------- | ------------------------------------------------------------------------------ |
| `bn.hello`               | `versions`, `client`, `locale?`, `capabilities?`          | `version`, `epoch`, `engine`, `limits`, `delivery?`, `locale`, `engine_locale` |
| `bn.viewport`            | `cols`, `rows`                                            | 없음                                                                           |
| `bn.interrupt`           | 없음                                                      | 없음                                                                           |
| `bn.hold`                | `epoch`, `screen_id`, `affordance_id`                     | 없음                                                                           |
| `bn.release`             | 없음                                                      | 없음                                                                           |
| `bn.subscribe`           | 없음                                                      | 스냅샷 헤더, 이어서 파트와 이벤트                                              |
| `bn.unsubscribe`         | 없음                                                      | 없음                                                                           |
| `bn.interaction.choices` | `epoch`, `boundary_id`, `component_id`, `offset`, `limit` | `boundary_id`, `component_id`, `total`, `rows` (읽기 전용)                     |
| `bn.world.describe`      | `epoch`, `boundary_id`, `pos`                             | `lines` (읽기 전용)                                                            |
| `bn.world.cells`         | `epoch`, `min`, `max`                                     | `at`, `cells`, `forgotten` (읽기 전용)                                         |
| `bn.overmap.cells`       | `epoch`, `min`, `max`                                     | 오버맵 레벨의 `overmap` 칸 (읽기 전용)                                         |
| `bn.command.submit`      | `epoch`, `expect`, `operation`                            | `command_id`, `stage: "received"`                                              |
| `bn.command.result`      | `epoch`, `command_id`                                     | 최신 단계, 유실된 알림 복구용                                                  |
| `bn.credit`              | `frames`                                                  | 지급 후 보유한 `credits`                                                       |

엔진이 보내는 알림: `bn.snapshot.part`, `bn.events`, `bn.command`, `bn.resync`, `bn.loading`, `bn.progress`.

오류는 코드 `1000`과 `error.data = {kind, action?, at?}`를 사용합니다. `action`은 다음에 할 일(`hello`, `subscribe`, `retry`)을 알려 줍니다.

## 화면

모든 경계에는 열려 있는 화면을 바깥쪽부터 나열한 `stack`이 들어 있습니다. 마지막 화면에만 affordance가 있습니다. 지도(role `map`)가 마지막이면 다른 화면은 입력을 받지 않습니다. 동작하려면 `operation: {kind: "invoke", affordance_id, args?}`와 해당 affordance의 `expect` 범위를 담아 `bn.command.submit`을 보냅니다.

- `screen`: `{screen_id}`. 해당 화면이 맨 위에 있는 동안만 유효하며, 아니면 `stale_screen`입니다.
- `boundary`: `{boundary_id, revision}`. 명령이 실행되는 동안 submit은 `command_busy`로 실패합니다.

자체 설명이 없는 화면은 role이 `screen`이고, kind는 입력 컨텍스트 이름을 소문자로 바꾼 값이며, 터미널 셀이 `canvas`로 들어갑니다. `avatar.sidebar`는 네이티브 사이드바를 같은 셀 형태로 담은 것입니다.

## 명령

`operation.kind`에는 `action`(등록된 액션)과 `travel`(지도 클릭)도 쓸 수 있으며, 이들은 `{boundary_id, revision}`을 기대합니다. 오래된 `expect`는 거부되므로, 클라이언트가 보지 못한 화면에서 동작하는 일은 없습니다.

단계: `received`, `validated`, `executing`, `completed`. 또는 `rejected`. 또는 `interrupted`이며, 이유는 `error`에 들어 있습니다.

## 전달 크레딧

`bn.hello`의 `delivery = {credits, max_hz?}`로 활성화합니다. 푸시되는 `bn.events` 또는 `bn.command` 프레임은 크레딧을 1 소모합니다. 크레딧이 없으면 엔진은 `bn.credit`이 올 때까지 프레임을 보류합니다. 보류 한도를 넘으면 reason이 `backpressure`인 `bn.resync`를 보냅니다.

## 프레이밍

프레임은 한 줄에 담긴 JSON 값 하나이며, 길이는 `limits.frame_bytes` 이하입니다. JSON이 아닌 줄에는 `-32700`, 형태가 잘못된 JSON에는 `-32600`, 알 수 없는 `bn.` 메서드에는 `-32601`, 잘못된 params에는 `-32602`로 응답합니다. 크기를 넘거나 중간에 잘린 프레임은 종료 코드 1로 엔진을 끝냅니다.

## 단일 시계

- `epoch`는 프로세스가 시작되거나 월드가 교체될 때 바뀝니다.
- `sequence`는 발행된 이벤트 수입니다(10진수 문자열, 연속). `revision`은 상태를 바꾸는 이벤트 수입니다. `at = {epoch, sequence, revision}`은 "sequence까지의 모든 이벤트를 포함함"을 뜻합니다.

## 값

- `pos = {dim, x, y, z}`는 절대 지도 칸입니다. `dim`은 게임 차원이며 기본 차원은 `""`입니다.
- `look = {kind, id, glyph, color, name}`은 게임 데이터의 외형을 담으므로, 텍스트 클라이언트에는 타일셋이 필요 없습니다.
- 이벤트 `changes`는 `coverage`, `view`, `cells`, `forgotten`, `entities`, `gone` 순으로 적용하고, 이어서 `avatar`, `environment`, `route`, `interaction`을 교체합니다.

---
title: 엔진/클라이언트 프로토콜 1.0
---

외부 클라이언트는 `cataclysm-bn-tiles --client=mcp`의 stdio에서 줄바꿈으로 구분된 JSON-RPC 2.0으로 게임을 플레이합니다. 값은 [Draft 2020-12 스키마](../../../schema/engine-client/1.0.schema.json)가 정의하며, 아래 예시는 모두 [예시 파일](../../../schema/engine-client/1.0.examples.json)에서 스키마로 검증됩니다. 기존 MCP 도구(`bn.observe`, `bn.press` 등)는 별개이며 바뀌지 않습니다. 클라이언트가 `bn.hello`를 호출하면 이 도구들은 입력을 거부합니다.

## 세션

```text
-> {"jsonrpc":"2.0","id":1,"method":"bn.hello","params":{"versions":["1.0"],"client":{"name":"my-client","version":"1"}}}
<- {"jsonrpc":"2.0","id":1,"result":{"version":"1.0","epoch":"epoch:e7f3","engine":{"build":"...","mods":["bn"]},"limits":{"frame_bytes":1048576,"cells_per_part":512,"cells_per_query":4096}}}
-> {"jsonrpc":"2.0","id":2,"method":"bn.subscribe","params":{}}
<- {"jsonrpc":"2.0","id":2,"result":{"at":{"epoch":"epoch:e7f3","sequence":"40","revision":"31"},"interaction":{...},"entities":[],"parts":1}}
<- {"jsonrpc":"2.0","method":"bn.snapshot.part","params":{"epoch":"epoch:e7f3","at":{...},"index":0,"last":true,"cells":[...]}}
-> {"jsonrpc":"2.0","id":3,"method":"bn.command.submit","params":{"epoch":"epoch:e7f3","expect":{"revision":"31","boundary_id":"boundary:90","schema_id":null},"operation":{"kind":"action","action_id":"RIGHT"}}}
<- {"jsonrpc":"2.0","id":3,"result":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"received"}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"validated"}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"executing"}}
<- {"jsonrpc":"2.0","method":"bn.events","params":{"epoch":"epoch:e7f3","events":[{"sequence":"41","revision":"32","type":"interaction.changed","command":"c:5","changes":{...}}]}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"completed","at":{"epoch":"epoch:e7f3","sequence":"41","revision":"32"}}}
```

`parts` 개수만큼 스냅샷 파트를 모은 뒤 `bn.events`를 순서대로 적용합니다. 누락(gap), epoch 불일치, `bn.resync`가 오면 `bn.subscribe`를 다시 호출합니다. 새 스냅샷은 상태만 복원하며, `lost_after` 이후의 일시적 이벤트는 유실로 보고되고 다시 전송되지 않습니다.

## 메서드

| 메서드                   | 매개변수                                  | 결과                                          |
| ------------------------ | ----------------------------------------- | --------------------------------------------- |
| `bn.hello`               | `versions`, `client`, `viewport?`         | `version`, `epoch`, `engine`, `limits`        |
| `bn.viewport`            | `cols`, `rows`                            | 없음                                          |
| `bn.subscribe`           | 없음                                      | 스냅샷 헤더. 이어서 파트와 이벤트가 전송됨    |
| `bn.unsubscribe`         | 없음                                      | 없음                                          |
| `bn.interaction.choices` | `epoch`, `boundary_id`, `offset`, `limit` | `boundary_id`, `total`, `choices` (읽기 전용) |
| `bn.world.cells`         | `epoch`, `min`, `max`                     | `at`, `cells`, `forgotten` (읽기 전용)        |
| `bn.command.submit`      | `epoch`, `expect`, `operation`            | `command_id`, `stage: "received"`             |
| `bn.command.result`      | `epoch`, `command_id`                     | 최신 단계. 유실된 알림을 복구할 때 사용       |

엔진이 보내는 알림은 `bn.snapshot.part`, `bn.events`, `bn.command`, `bn.resync`, `bn.loading`입니다. `bn.loading`을 제외한 알림은 구독한 클라이언트에 입력 경계마다 전송됩니다.

오류는 코드 `1000`과 `error.data = {kind, action?, at?}`를 사용하며, `action`은 다음에 할 일(`hello`, `subscribe`, `retry`)을 알려 줍니다.

## 하나의 시계

- `epoch`는 프로세스가 시작되거나 월드가 교체될 때 바뀌며, 읽기에서는 바뀌지 않습니다.
- `sequence`는 게시된 이벤트 수입니다(10진 문자열, 연속, `"1"`부터 시작). `revision`은 상태를 바꾸는 이벤트 수입니다. `at = {epoch, sequence, revision}`은 "sequence까지의 모든 이벤트를 포함한다"는 뜻입니다.
- 명령은 `expect = {revision, boundary_id, schema_id}`를 가집니다. 오래된 값은 거부되므로 클라이언트는 보지 못한 화면에 대해 행동하지 않습니다. `schema_id`는 해당 경계에 상호작용이 없을 때만 `null`입니다.

## 명령

`operation.kind`는 `choose`, `fill`, `set_count`, `set_target`, `cancel`(의미 기반 메뉴) `action`(이동 키 같은 등록된 행동) 또는 `travel`(맵 클릭) 중 하나입니다.

```json
{ "kind": "choose", "choice_id": "tab:new_game" }
{ "kind": "set_target", "pos": { "dim": "", "x": 9, "y": 4, "z": 0 } }
{ "kind": "action", "action_id": "RIGHT" }
{ "kind": "travel", "pos": { "dim": "", "x": 9, "y": 4, "z": 0 } }
```

`travel`은 상호작용이 없는 경계에서 화면에 보이는 절대 맵 칸에 대한 Tiles와 curses의 왼쪽 클릭입니다. 첫 클릭은 엔진 자체의 경로를 `route`로 게시하고, 같은 칸을 다시 클릭하면 네이티브 자동 이동이 시작됩니다. 이후 아바타는 한 걸음씩 걸으며 걸음마다 해당 명령에 연결된 일반 이벤트가 게시되고, 엔진이 멈추는 곳(도착, 몬스터 시야 진입 등)에서 멈춥니다. 걷기가 끝나 다시 입력을 기다리면 명령은 `completed`가 됩니다. 다른 명령이나 다른 칸 클릭은 계획을 바꾸거나 지웁니다. 클라이언트는 경로를 직접 계산하지 않습니다.

단계는 `received`, `validated`, `executing`, `completed`이며, 또는 `rejected`, 또는 `interrupted`이며, `interrupted`의 `error`가 이유를 알려 줍니다(월드가 교체되면 `stale_epoch`, 게임이 입력을 거부하면 `validation_failed`, 그 외에는 `not_ready`). `completed`는 입력이 다시 대기 중이라는 뜻입니다. 대기 같은 활동은 끝나거나 게임이 무언가를 물을 때(팝업, 중단)까지 먼저 실행됩니다. 그 `at`은 해당 경계의 끝 지점입니다. 한 번에 하나의 명령만 진행할 수 있습니다(`command_busy`).

## 로딩 화면

월드를 불러오는 동안 게임 스레드는 작업 중이라 입력 경계에 도달하지 않으므로, `bn.hello`를 마친 클라이언트는 네이티브 로딩 화면의 단계마다 `bn.loading`을 받습니다(구독 여부와 무관). 진행 상황은 네이티브 클라이언트가 보여 주는 화면과 같습니다. `title`은 현재 단계(예: "Loading files"), `entries`는 그 목록, `index`는 진행 중인 항목(앞선 항목은 완료), `image`는 네이티브 클라이언트와 같은 방식으로 고른 로딩 이미지입니다. `image.path`는 게임 기본 경로 기준 상대 경로이며, 파일 이름에 제작자가 없으면 `author`는 생략됩니다. 로딩 화면이 끝나면 `done`이 한 번 전송되고, 그 뒤에 평소처럼 `bn.resync`나 `bn.events`가 이어집니다. `epoch`는 해당 단계를 보낼 때의 epoch입니다.

```text
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","title":"Loading files","entries":["Terrain","Items"],"index":1,"image":{"path":"data/json/loading/Ada_dawn.webp","author":"Ada"}}}
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","done":true}}
```

## 값

- `pos = {dim, x, y, z}`는 절대 맵 칸이며, `dim`은 게임 차원이고 기본 차원은 `""`입니다. 현실 거품 좌표는 전송 형식에 나타나지 않습니다.
- `look = {kind, id, glyph, color}`는 게임 데이터의 외형을 담으므로 텍스트 클라이언트에는 타일셋이 필요 없습니다. 자동 벽 지형은 아바타가 아는 연결 상태에 맞는 선 문자를 담으며, 기억된 칸은 `memory.terrain` 위에 `memory.overlay`(가구, 함정, 차량 부품)를 유지합니다.
- `interaction`은 네이티브 메뉴나 대화상자입니다. `choices`는 처음 최대 200행을 담지만 크기 때문에 더 적을 수 있으므로, 나머지는 `bn.interaction.choices`를 `offset = choices.length`부터 읽습니다. 전체 행 수는 `choice_total`입니다. `compat.focus`와 `compat.panes`는 1:1 이식을 위해 네이티브 목록 상태를 유지하며 클라이언트는 무시해도 됩니다. 메인 메뉴는 네이티브와 같이 탭 구조입니다. 각 탭은 `pane_id`가 없는 선택지(`tab:new_game`, `tab:load` 등, 선택된 탭은 `selected`)이고, 그 뒤에 선택된 탭의 항목만 해당 탭의 `pane_id`와 함께 이어집니다(`new_game:tutorial`, `settings:options`, `load:<world>`). 다른 탭을 선택하면 그 탭의 항목을 담은 새 interaction이 게시됩니다. ID는 위치나 언어에 의존하지 않습니다.
- 월드는 아바타가 아는 것입니다. `cells`(`remembered`, `visible`, `sensed`), `entities`, `avatar`(능력치와 소지품 `inventory`), `environment`, 로드된 `coverage`, `view`, 대기 중인 `route`(계획이 없으면 비어 있거나 없음)로 구성됩니다. `view`는 아바타가 있는 층에서 네이티브 지형 창이 보여 주는 `{min, max}` 칸입니다(아바타를 따라 움직임). `travel` 클릭은 이 안에서만 받아들여지고, 밖이면 `validation_failed`로 거부됩니다.
- `avatar.sidebar`는 같은 getter에서 가져온 네이티브 클래식 사이드바 값입니다. `limbs`(`id`, `label`과 상태 색, `hp`, `hp_max`, 막대 `color`, `broken`), `pain`, `hunger`, `thirst`, `fatigue`, `temperature`, `power`, `location`, `weather`(네이티브 색 이름의 `{text, color}`), `focus`, `morale`(`level`, 네이티브 `face`), `stamina`, `speed`, `movement`(`counter`, `mode`는 `walk`, `run`, `crouch`, `prone`), `safe_mode`, `time`(`season`, `day`, `clock`: 시계 시각, 없으면 대략의 시간대, 지하에서는 빈 문자열), 선택적 `ambient_temperature`(온도계가 있을 때만), `weapon`, 선택적 `style`(무술 스타일)로 구성됩니다. 문자열은 엔진의 표시 언어이고 나머지 값은 언어 중립입니다. `stats` 항목은 네이티브 `color`를 가질 수 있습니다.
- 지형 창은 네이티브 창 크기 변경과 같은 방식으로 정해집니다. `bn.hello`의 `viewport = {cols, rows}`(맵 칸 1~512) 또는 이후의 `bn.viewport`는 터미널을 그 칸 수에 옆 패널을 더한 크기로 바꿔, 엔진이 아바타 주변의 그만큼을 보여 주게 합니다. 네이티브 최소 터미널 크기가 여전히 적용되므로 실제 `view`가 기준입니다. 아무것도 보내지 않으면 기본 터미널이 쓰입니다.
- 모든 이벤트는 일반 `changes` 블록을 가지며, `coverage`, `view`, `cells`, `forgotten`, `entities`, `gone` 순으로 적용한 뒤 `avatar`, `environment`, `route`, `interaction`을 교체합니다.

이벤트는 `interaction.changed`, `coverage.moved`, `turn.passed`, `cells.seen`이며, 이후 스키마에 나열된 연출 유형이 추가됩니다. 새 이벤트 유형에는 새로운 정확한 버전이 필요합니다.

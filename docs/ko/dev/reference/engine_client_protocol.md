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

| 메서드                   | 매개변수                                    | 결과                                          |
| ------------------------ | ------------------------------------------- | --------------------------------------------- |
| `bn.hello`               | `versions`, `client`, `viewport?`, `tiles?` | `version`, `epoch`, `engine`, `limits`        |
| `bn.viewport`            | `cols`, `rows`                              | 없음                                          |
| `bn.interrupt`           | 없음                                        | 없음                                          |
| `bn.subscribe`           | 없음                                        | 스냅샷 헤더. 이어서 파트와 이벤트가 전송됨    |
| `bn.unsubscribe`         | 없음                                        | 없음                                          |
| `bn.interaction.choices` | `epoch`, `boundary_id`, `offset`, `limit`   | `boundary_id`, `total`, `choices` (읽기 전용) |
| `bn.world.describe`      | `epoch`, `boundary_id`, `pos`               | `lines` (읽기 전용)                           |
| `bn.world.cells`         | `epoch`, `min`, `max`                       | `at`, `cells`, `forgotten` (읽기 전용)        |
| `bn.command.submit`      | `epoch`, `expect`, `operation`              | `command_id`, `stage: "received"`             |
| `bn.command.result`      | `epoch`, `command_id`                       | 최신 단계. 유실된 알림을 복구할 때 사용       |

엔진이 보내는 알림은 `bn.snapshot.part`, `bn.events`, `bn.command`, `bn.resync`, `bn.loading`, `bn.progress`입니다. `bn.loading`과 `bn.progress`를 제외한 알림은 구독한 클라이언트에 입력 경계마다 전송됩니다. `bn.progress`(`{epoch}`)는 하트비트로, 대기 같은 활동이 입력 경계에 도달하지 않고 계속되는 동안 `bn.hello`를 마친 클라이언트에 2초마다 전송됩니다. 통신이 끊기면 엔진이 일하지 않는다는 뜻입니다. 월드를 불러오는 동안은 `bn.loading`이 같은 역할을 합니다.

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

모든 경계 액션은 활성 입력 컨텍스트가 그 액션에 묶은 단일 키보드 키의 이식 가능한 이름(`ESC`, `SPACE`, `RETURN`, `UP`, `>` 등)을 `keys`로 나열합니다. 키가 없는 액션은 `action`으로 실행할 수 없습니다. 눌린 키로 동작하려는 클라이언트는 키 설정 파일에서 추측하지 말고 `keys`에 그 키가 있는 액션을 찾아야 합니다.

`travel`은 상호작용이 없는 경계에서 화면에 보이는 절대 맵 칸에 대한 Tiles와 curses의 왼쪽 클릭입니다. 첫 클릭은 엔진 자체의 경로를 `route`로 게시하고, 같은 칸을 다시 클릭하면 네이티브 자동 이동이 시작됩니다. 이후 아바타는 한 걸음씩 걸으며 걸음마다 해당 명령에 연결된 일반 이벤트가 게시되고, 엔진이 멈추는 곳(도착, 몬스터 시야 진입 등)에서 멈춥니다. 걷기가 끝나 다시 입력을 기다리면 명령은 `completed`가 됩니다. 다른 명령이나 다른 칸 클릭은 계획을 바꾸거나 지웁니다. 클라이언트는 경로를 직접 계산하지 않습니다.

`context`는 같은 칸에 대한 오른쪽 클릭(SEC_SELECT)입니다. 네이티브에서는 인접한 칸을 조사하거나, 인접한 문을 닫거나, 발밑 아이템을 줍거나, 보이는 몬스터를 쏘거나, 계획된 경로를 먼저 취소합니다. `bn.world.describe`는 터미널 창 안의 칸에 마우스를 올렸을 때 네이티브 마우스 뷰가 출력하는 텍스트를 돌려줍니다(범위 밖은 `validation_failed`). 아무것도 선택하거나 계획하지 않습니다. 메뉴의 휠 한 단계는 등록된 행동 `SCROLL_UP` 또는 `SCROLL_DOWN`이며 엔진이 네이티브 커서를 세 행 옮깁니다.

단계는 `received`, `validated`, `executing`, `completed`이며, 또는 `rejected`, 또는 `interrupted`이며, `interrupted`의 `error`가 이유를 알려 줍니다(월드가 교체되면 `stale_epoch`, 게임이 입력을 거부하면 `validation_failed`, 그 외에는 `not_ready`). `completed`는 입력이 다시 대기 중이라는 뜻입니다. 대기 같은 활동은 끝나거나 게임이 무언가를 물을 때(팝업, 중단)까지 먼저 실행됩니다. 그 `at`은 해당 경계의 끝 지점입니다. 한 번에 하나의 명령만 진행할 수 있습니다(`command_busy`). 대기 같은 활동이 실행되는 동안에도 요청에는 응답합니다. `bn.interrupt`는 네이티브 중단 키를 눌러 활동이 Tiles처럼 중지할지 묻게 하며, 그 질문이 다음 경계가 됩니다. 활동 중이 아니면 `not_ready`로 실패합니다.

## 로딩 화면

월드를 불러오는 동안 게임 스레드는 작업 중이라 입력 경계에 도달하지 않으므로, `bn.hello`를 마친 클라이언트는 네이티브 로딩 화면의 단계마다 `bn.loading`을 받습니다(구독 여부와 무관). 진행 상황은 네이티브 클라이언트가 보여 주는 화면과 같습니다. `title`은 현재 단계(예: "Loading files"), `entries`는 그 목록, `index`는 진행 중인 항목(앞선 항목은 완료), `image`는 네이티브 클라이언트와 같은 방식으로 고른 로딩 이미지입니다. `image.path`는 게임 기본 경로 기준 상대 경로이며, 파일 이름에 제작자가 없으면 `author`는 생략됩니다. 로딩 화면이 끝나면 `done`이 한 번 전송되고, 그 뒤에 평소처럼 `bn.resync`나 `bn.events`가 이어집니다. `epoch`는 해당 단계를 보낼 때의 epoch입니다.

```text
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","title":"Loading files","entries":["Terrain","Items"],"index":1,"image":{"path":"data/json/loading/Ada_dawn.webp","author":"Ada"}}}
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","done":true}}
```

## 값

- `pos = {dim, x, y, z}`는 절대 맵 칸이며, `dim`은 게임 차원이고 기본 차원은 `""`입니다. 현실 거품 좌표는 전송 형식에 나타나지 않습니다.
- `look = {kind, id, glyph, color}`는 게임 데이터의 외형을 담으므로 텍스트 클라이언트에는 타일셋이 필요 없습니다. 자동 벽 지형은 아바타가 아는 연결 상태에 맞는 선 문자를 담으며, 기억된 칸은 `memory.terrain` 위에 `memory.overlay`(가구, 함정, 차량 부품)를 유지합니다.
- `look`은 `name`, 즉 네이티브 look이 보여 주는 번역된 플레이어용 이름(`t_floor`는 "wooden floor")을 담으므로 텍스트·음성 클라이언트가 ID를 읽을 필요가 없습니다. 생물은 엔티티의 `name`을 씁니다.
- 타일 클라이언트는 같은 `look`을 게임 타일셋으로 그립니다. 엔진만 알 수 있는 정보(데이터와 네이티브 선택 로직을 가진 쪽이 엔진이므로)를 공개하고, 클라이언트는 이를 다시 구현하지 않습니다: `tile`(`id`와 다른 스프라이트 ID: `corpse_<monster>`, `player_male`, `vp_<part>`), `looks_like`(데이터의 대체 체인, 가까운 순), `subtile`과 `rotation`(아바타가 아는 이웃 칸으로 네이티브 선택이 계산한 멀티타일 키와 90도 회전 수이므로 연결 로직은 한 곳에만 존재), `stack`(더미의 표시 아이템), `attitude`와 `aware`(몬스터와 NPC, 네이티브 화면이 표식을 겹쳐 그림), `facing`(생물), 칸의 `light`(네이티브 `lit_level`: 0 어두움, 1 낮음, 2 밝기만, 3 조명, 4 밝음), `environment.season`, 아바타의 `look`, 캐릭터의 `overlays`(착용·들고 있는 아이템, 돌연변이, 바이오닉을 네이티브 순서로 시도할 스프라이트 ID를 `tile`과 `looks_like`에 담은 종류 `overlay`의 `look`)입니다. 파일, 가중치 변형, 레이어, 어둡게 하기는 타일셋이 정의하므로 클라이언트가 맡습니다. 그리기 순서는 지형, 가구, 함정, 필드, 아이템, 차량 부품, 생물이며 `fields`와 `items`의 마지막 항목이 네이티브 화면에 그려지는 것입니다. 기억된 칸은 어둡게, `light`가 1이면 음영으로 그리고 스프라이트가 없는 ID는 `glyph`로 대체합니다.
- `interaction`은 네이티브 메뉴나 대화상자입니다. `choices`는 처음 최대 200행을 담지만 크기 때문에 더 적을 수 있으므로, 나머지는 `bn.interaction.choices`를 `offset = choices.length`부터 읽습니다. 전체 행 수는 `choice_total`입니다. `compat.focus`와 `compat.panes`는 1:1 이식을 위해 네이티브 목록 상태를 유지하며 클라이언트는 무시해도 됩니다. 메인 메뉴는 네이티브와 같이 탭 구조입니다. 각 탭은 `pane_id`가 없는 선택지(`tab:new_game`, `tab:load` 등, 선택된 탭은 `selected`)이고, 그 뒤에 선택된 탭의 항목만 해당 탭의 `pane_id`와 함께 이어집니다(`new_game:tutorial`, `settings:options`, `load:<world>`). 다른 탭을 선택하면 그 탭의 항목을 담은 새 interaction이 게시됩니다. ID는 위치나 언어에 의존하지 않습니다. 제작 메뉴(`context`가 `CRAFTING`)도 같은 탭 구조입니다. 분류 탭(`tab:CC_ELECTRONIC`), 선택된 분류의 하위 탭(`subtab:CSC_ELECTRONIC_TOOLS`, 검색 중에는 없음), 레시피(`recipe:<hash>`) 순이며 뒤의 둘은 분류를 `pane_id`로 가집니다. 탭이나 하위 탭을 선택하면 그 묶음이 나열되고, 레시피를 선택하면 제작하며, 레시피에 `set_count`를 보내면 일괄 제작에 들어갑니다. 각 레시피는 네이티브 상세 텍스트(기술, 시간, 도구, 재료)를 색 태그 없이 `description`에 담습니다. `filter` 필드가 네이티브 검색이며, `submit: true`로 `fill`하면 검색하고 빈 값이면 해제합니다.
- 월드는 아바타가 아는 것입니다. `cells`(`remembered`, `visible`, `sensed`), `entities`, `avatar`(능력치, 소지품 `inventory`, 발밑 `ground`), `environment`, 로드된 `coverage`, `view`, 대기 중인 `route`(계획이 없으면 비어 있거나 없음)로 구성됩니다. `view`는 아바타가 있는 층에서 네이티브 지형 창이 보여 주는 `{min, max}` 칸입니다(아바타를 따라 움직임). `travel` 클릭은 이 안에서만 받아들여지고, 밖이면 `validation_failed`로 거부됩니다.
- `avatar.inventory`는 손에 든 아이템, 착용한 아이템, 들고 있는 스택 순서이며 각 항목은 `{look, name, count?, slot}`이고 `slot`은 `wielded`, `worn`, `carried` 중 하나입니다. `avatar.ground`는 아바타가 선 칸의 스택을 `slot` 없이 같은 형태로 나열합니다. `name`은 네이티브 목록이 보여 주는 표시 이름이며 색 마크업을 포함하지 않습니다. 두 배열 모두 비어 있으면 생략됩니다.
- `interaction`은 네이티브 메뉴나 대화상자입니다. `choices`는 처음 최대 200행을 담지만 크기 때문에 더 적을 수 있으므로, 나머지는 `bn.interaction.choices`를 `offset = choices.length`부터 읽습니다. 전체 행 수는 `choice_total`입니다. `compat.focus`와 `compat.panes`는 1:1 이식을 위해 네이티브 목록 상태를 유지하며 클라이언트는 무시해도 됩니다. 메인 메뉴는 네이티브와 같이 탭 구조입니다. 각 탭은 `pane_id`가 없는 선택지(`tab:new_game`, `tab:load` 등, 선택된 탭은 `selected`)이고, 그 뒤에 선택된 탭의 항목만 해당 탭의 `pane_id`와 함께 이어집니다(`new_game:tutorial`, `settings:options`, `load:<world>`). 다른 탭을 선택하면 그 탭의 항목을 담은 새 interaction이 게시됩니다. ID는 위치나 언어에 의존하지 않습니다.
- 오버맵(`m`, 액션 `map`)은 `context`가 `OVERMAP`인 `target` 종류의 interaction입니다. `overmap`은 네이티브 창을 그려진 그대로 담습니다. `cols`, `rows`, `cells[행][열] = [글리프, 전경, 배경]`(curses 색 인덱스 0~15: 0 검정, 1 빨강, 2 초록, 3 갈색, 4 파랑, 5 마젠타, 6 청록, 7 밝은 회색, 8~15는 각 색의 밝은 형태), 왼쪽 위 칸의 칸을 가리키는 `origin`, `player`, 사이드바 글을 담은 `legend` 줄, 화면 안의 `notes`입니다. 창은 커서 `target.current`를 중심으로 하고 `target.unit`은 `"omt"`이므로 이 interaction의 모든 위치는 맵 칸이 아니라 절대 오버맵 지형 칸입니다. `pos`가 있는 `set_target`은 커서를 옮기고(엔진 범위 안의 어떤 칸이든, 층은 -10~10), `cancel`은 화면을 닫으며, 네이티브 오버맵 동작(이동의 `CHOOSE_DESTINATION`, `CREATE_NOTE`, `LEVEL_UP`/`LEVEL_DOWN`, `ZOOM_IN`, 각종 토글 등)은 그 경계의 등록된 액션입니다. 클라이언트는 깜박일 수 없으므로 오버레이(플레이어, 메모, 경로)는 계속 표시됩니다. `path`는 계획된 이동 경로입니다. `bn.hello`에서 `"tiles": true`라고 알린 클라이언트는 `tiles`도 받습니다. 칸마다 `[tile, rotation, subtile]`(네이티브 타일 화면이 고르는 `overmap_terrain` 타일 id, 90도 회전 수, 멀티타일 키)과 각 타일 id의 `looks_like` 대체 목록입니다. 다른 클라이언트에는 보내지 않습니다.
- 설정 화면은 네이티브 화면입니다. `settings:options`, `settings:keybindings`, `settings:autopickup`, `settings:safemode`, `settings:distractions`, `settings:colors`로 열며, 옵션과 키 설정은 게임 안에서도 열립니다. `editor`가 있는 행은 직접 설정할 수 있습니다. `fill`의 `field_id`에 행의 `choice_id`, `value`에 편집기 형식의 값, `submit: true`를 보냅니다. `editor.type`은 `bool`(`true`/`false`), `select`(`values[].id` 중 하나), `integer`/`float`(`minimum`~`maximum`), `text`(`max_length` 이하)이며 네이티브 메뉴가 받지 않는 값은 엔진이 거부합니다. 행에 대한 `choose`는 네이티브 Enter와 같습니다. 옵션: 탭 행 `page:<id>` 뒤에 선택한 페이지의 `option:<NAME>`, `group:<id>` 행이 옵니다. `cancel`로 나가면 네이티브 "변경 사항을 저장하시겠습니까?"가 나오고, 저장하면 적용되고 기록됩니다. 키 설정: `mode:add_local`, `mode:add_global`, `mode:remove`(네이티브가 제공하면 `mode:execute`)가 다음 `action:<ID>` 행의 동작을 정하고, 필드는 이름으로 행동을 거르며, 새 키를 묻는 프롬프트에는 `key` 형식 필드가 있어 키 설정 파일과 같은 표기(`z`, `=`, `F9`, `CTRL+A`)의 키 이름을 받습니다. 자동 줍기와 안전 모드: 탭 행 `page:<n>`, 열 행 `column:<n>`, `rule:<n>` 행이며 규칙 동작(추가, 제거, 이동 등)은 화면의 등록된 행동입니다. 주의 분산: `bool` 편집기가 있는 `distraction:<name>` 행(true면 중단). 색상: `column:<n>`과 `color:<name>` 행이며 색을 고르면 네이티브 색상 목록이 열립니다.
- 월드는 아바타가 아는 것입니다. `cells`(`remembered`, `visible`, `sensed`), `entities`, `avatar`(능력치와 소지품 `inventory`), `environment`, 로드된 `coverage`, `view`, 대기 중인 `route`(계획이 없으면 비어 있거나 없음)로 구성됩니다. `view`는 아바타가 있는 층에서 네이티브 지형 창이 보여 주는 `{min, max}` 칸입니다(아바타를 따라 움직임). `travel` 클릭은 이 안에서만 받아들여지고, 밖이면 `validation_failed`로 거부됩니다.
- `avatar.sidebar`는 같은 getter에서 가져온 네이티브 클래식 사이드바 값입니다. `limbs`(`id`, `label`과 상태 색, `hp`, `hp_max`, 막대 `color`, `broken`), `pain`, `hunger`, `thirst`, `fatigue`, `temperature`, `power`, `location`, `weather`(네이티브 색 이름의 `{text, color}`), `focus`, `morale`(`level`, 네이티브 `face`), `stamina`, `speed`, `movement`(`counter`, `mode`는 `walk`, `run`, `crouch`, `prone`), `safe_mode`, `time`(`season`, `day`, `clock`: 시계 시각, 없으면 대략의 시간대, 지하에서는 빈 문자열), 선택적 `ambient_temperature`(온도계가 있을 때만), `weapon`, 선택적 `style`(무술 스타일)로 구성됩니다. 문자열은 엔진의 표시 언어이고 나머지 값은 언어 중립입니다. `stats` 항목은 네이티브 `color`를 가질 수 있습니다.
- 지형 창은 네이티브 창 크기 변경과 같은 방식으로 정해집니다. `bn.hello`의 `viewport = {cols, rows}`(맵 칸 1~512) 또는 이후의 `bn.viewport`는 터미널을 그 칸 수에 옆 패널을 더한 크기로 바꿔, 엔진이 아바타 주변의 그만큼을 보여 주게 합니다. 네이티브 최소 터미널 크기가 여전히 적용되므로 실제 `view`가 기준입니다. 아무것도 보내지 않으면 기본 터미널이 쓰입니다.
- 모든 이벤트는 일반 `changes` 블록을 가지며, `coverage`, `view`, `cells`, `forgotten`, `entities`, `gone` 순으로 적용한 뒤 `avatar`, `environment`, `route`, `interaction`을 교체합니다.
- `message.logged`는 네이티브 메시지 로그입니다. `data = {id, text, kind, color, count}`이며, 한 줄마다 이벤트 하나가 로그 순서대로, 원인이 된 명령에 연결되어 전송됩니다. `text`는 색 태그와 "x N"을 뺀 번역된 줄, `kind`는 네이티브 유형(`good`, `bad`, `mixed`, `warning`, `info`, `neutral`, `debug`), `color`는 그 유형의 네이티브 색 이름입니다. 마지막 줄이 반복되면 같은 `id`가 더 큰 `count`로 다시 전송되어 그 줄을 대체합니다. 상태를 바꾸지 않으며(`revision` 불변) 다른 일시 이벤트처럼 `bn.resync`를 거치면 사라집니다. 네이티브 로그가 쿨다운으로 숨기는 메시지는 전송되지 않습니다.
- `projectile.moved`, `explosion.started`, `explosion.blast`, `explosion.ended`, `combat_text.shown`은 네이티브 애니메이션 사실입니다. 게임이 애니메이션을 시작하는 지점에서 게시되므로 모든 클라이언트가 렌더러 없이 같은 사실을 받습니다. 상태를 바꾸지 않고, 원인 명령에 붙으며, 그 명령의 상태 이벤트보다 먼저 도착합니다. `display.duration_ms`는 네이티브 애니메이션이 그 사실을 유지하는 시간입니다(단계마다 `ANIMATION_DELAY` 옵션). `projectile.moved`는 `path`(그리는 순서의 칸. 탄이 날아가는 동안은 이벤트마다 한 칸)와 글리프와 사용자 지정 스프라이트 ID를 가진 `projectile` 종류의 `look`을 담습니다. `explosion.started`는 `at`, `radius`, `color`와 네이티브 타일이 쓰는 타일 ID `tile`을 담습니다. 사실은 네이티브 옵션을 따라 `ANIMATIONS`가 꺼져 있으면 아무것도 게시되지 않고, 전투 텍스트에는 `ANIMATION_SCT`도 필요합니다. `explosion.blast`는 지형에 따라 모양이 달라지는 폭발의 한 고리 `cells`를 나열합니다. `combat_text.shown`은 `at`에 뜨는 한두 색 `segments`의 떠오르는 글자로, 네이티브처럼 애니메이션 단계마다 `scroll`(단위 이동)만큼 움직입니다. `kind`가 `hp`이면 생물의 체력 표시이며 그 옆에 머무릅니다.

이벤트는 `interaction.changed`, `coverage.moved`, `turn.passed`, `cells.seen`, `message.logged`와 위의 애니메이션 사실이며, 이후 스키마에 나열된 나머지 연출 유형이 추가됩니다. 1.0 공개 전에 추가한 이벤트 유형은 1.0에 포함되며, 공개 후에는 새 이벤트 유형에 새로운 정확한 버전이 필요합니다.

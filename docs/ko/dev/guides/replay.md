---
title: 입력 기록과 재생
---

`--replay-record PATH`로 게임 세션을 기록하고 `--replay-play PATH`로 재생하세요. 이 옵션은 공유 입력 경계를 사용하므로 타일, Curses, MCP에서 작동합니다. 재생은 입력을 직접 제공하므로 MCP 기록을 재생할 때 MCP 호스트가 필요하지 않습니다.

```sh
out/build/linux-full/src/cataclysm-bn --client=tiles \
  --userdir out/record-user/ --configdir out/record-user/config/ \
  --seed example --replay-record out/example.jsonl
out/build/linux-full/src/cataclysm-bn --client=tiles \
  --userdir out/playback-user/ --configdir out/playback-user/config/ \
  --replay-play out/example.jsonl
```

**두 실행 모두 동일한 초기 사용자 디렉터리 복사본에서 시작하세요.** `--userdir`와 `--configdir`를 모두 전달하세요. 그렇지 않으면 XDG 빌드가 계정의 일반 구성을 계속 사용합니다. 실행하기 전에 같은 명령에 `--paths`를 추가해 해석된 경로를 확인하세요. 리플레이는 저장 스냅샷이 아니라 입력 기록입니다. 월드, 구성, 키 바인딩, 캐릭터 템플릿, 모드 데이터를 복원하지 않습니다. 일반 저장 파일을 대상으로 리플레이하지 마세요. 특히 플레이가 끝난 _뒤에_ 기록용 프로필을 복사해도 초기 상태는 재현되지 않습니다.

같은 게임 빌드, 게임 데이터, 모드, 옵션, 클라이언트, 초기 터미널 크기를 사용하세요. 헤더가 기록 시 RNG 시드를 제공하며 재생 시 명시적인 `--seed`를 지정하면 이 값과 일치해야 합니다. 두 리플레이 옵션은 함께 사용할 수 없습니다. 기록은 기존 파일을 덮어쓰지 않습니다.

## 기록되는 내용

버전이 지정된 JSONL 스트림에는 다음이 포함됩니다:

- 형식 버전과 결정적 RNG 시드를 담은 헤더.
- 키보드, 텍스트/IME, 마우스, 게임패드, 시맨틱 상호작용, 시간 제한 폴링, 빈 폴링 이벤트와 각 입력 컨텍스트, 등록된 액션, 폴링 시간 제한. 빈 비차단 폴링도 중요합니다. 이를 생략하면 활동 중 나중 입력이 너무 일찍 적용될 수 있습니다.
- 입력 스트림 밖에서 게임이 읽는 텍스트(붙여넣은 클립보드, `input.txt`)는 별도 레코드로 기록되며, 재생 시 현재 클립보드나 파일 대신 기록된 텍스트가 삽입됩니다.
- 마우스 이벤트에는 좌표 단위(Tiles는 픽셀, Curses/MCP는 셀)가 포함됩니다. 재생 시 다른 단위로 기록된 마우스 이벤트는 거부됩니다.
- 입력 개수를 담은 정상 종료 레코드.

이벤트는 기존 게임 입력 로직을 통과합니다. 턴 단위 액션뿐 아니라 중첩된 프롬프트도 기록됩니다. 기록과 재생 중에는 경과한 실시간 대신 각 시뮬레이션 호출 경계에서 중단 가능한 활동을 폴링합니다. 따라서 두 리플레이 모드의 빈 폴링 순서가 같아지며 일반 플레이는 100ms 폴링 제한을 유지합니다. 재생은 정상 종료 레코드에서 멈추고 사람의 입력을 기다리지 않은 채 성공적으로 종료됩니다. 저장하려면 기록 안에 게임 내 저장 액션이 여전히 필요합니다.

시맨틱 레코드는 작업, 불투명 선택지/필드/후보 ID, 항목 수량, 대상 위치를 보존하지만 전송 요청 번호와 비슷한 `input_id`는 보존하지 않습니다. 재생은 같은 위젯 구현을 통해 이 레코드를 전달하고 적용하기 전에 상호작용 종류, 스키마, 수량 제한, 대상 식별자와 사거리를 검증합니다. 따라서 클라이언트 간 재생을 허용하면서 재정렬된 인벤토리, 변경된 대상 또는 동일한 일반 입력 컨텍스트의 다른 목록이 작업을 소리 없이 소비하는 일을 막습니다.

클라이언트의 `pickup`과 `drop` 명령은 지도 화면의 시맨틱 레코드로 기록됩니다. 스키마 `item_operation:pickup` 또는 `item_operation:drop`의 `set_count`이며, 대상은 `avatar.ground` 또는 `avatar.inventory` 행 ID, 수량은 그 행의 네이티브 단위입니다. 재생할 때 같은 줍기·버리기 활동을 시작합니다.

컨텍스트 불일치, 상호작용 스키마 불일치, 알 수 없는 형식 버전, 잘못된 이벤트, 일치하지 않는 개수, 종료 레코드 누락은 오류입니다. 진단에는 파일과 레코드 줄이 표시됩니다. 기록된 입력을 모두 소비하기 전에 게임이 종료되어도 실패합니다. 재생은 전체 기록을 메모리에 불러오지 않고 한 번에 레코드 하나씩 읽습니다.

결정적 RNG 지원은 일반 병렬 작업을 유지하면서 키가 지정된 워커 작업과 병렬 루프 인덱스에 독립적인 난수 스트림을 제공합니다. 서로 다른 게임 버전, 플랫폼, 표준 라이브러리, CPU/GPU 계산 정책 또는 모든 비동기 게임 시스템에서 결과가 같다는 보장은 아닙니다. 입력 컨텍스트 검증은 상호작용 변화를 감지하지만 전체 시뮬레이션 상태 체크섬은 아닙니다.

## 두 빌드 비교

한 번 기록한 뒤, 그 기록을 비교 기준 빌드와 검사 대상 빌드에서 재생합니다:

```sh
deno task replay:record --binary ./cataclysm-bn-tiles out/rec
deno task replay:diff --recording out/rec/inputs.jsonl --profile out/rec/profile \
  --base base/cataclysm-bn-tiles --head ./cataclysm-bn-tiles out/diff
```

`replay:record`는 `--entry`(기본값 `new_game:tutorial`)의 새 게임을 시작해 `--waits`번 기다린 뒤 저장하고 종료합니다. `out/rec/profile`은 기록이 시작된 시점의 손대지 않은 사용자 디렉터리입니다. `replay:diff`는 각 빌드에서 그 프로필의 사본으로 `--state-hash-log`를 켜고 기록을 재생한 뒤, 로그를 턴마다, 저장된 월드를 `tools/client/compare_saves.ts`로 비교합니다. `out/diff/summary.json`에는 턴이나 `rng`, `avatar`, `monsters`, `npcs`, `map` 해시가 처음 달라지는 단계와 달라진 저장 레코드가 적힙니다. 두 빌드가 같으면 0, 갈라지면 1, 재생이 실패하면 2로 끝납니다(입력 컨텍스트가 바뀐 것도 실패이며 `game.stderr`가 그 이벤트를 알려 줍니다). 게임 루트가 바이너리 디렉터리가 아닌 빌드에는 `--base-root`와 `--head-root`를 줍니다.

## 튜토리얼 세션 기록

`--scenario tutorial`은 기다리기 뒤에 더 긴 세션을 클라이언트와 같은 프로토콜로 기록하고, 실행한 단계를 출력합니다:

```sh
deno task replay:record --binary ./cataclysm-bn-tiles --scenario tutorial out/tutorial
deno task replay:diff --recording out/tutorial/inputs.jsonl --profile out/tutorial/profile \
  --base base/cataclysm-bn-tiles --head ./cataclysm-bn-tiles out/tutorial-diff
```

세션은 인벤토리 선택기로 정확한 수량을 버리고, 고급 인벤토리에서 필터와 이동을 하며, 마체테를 줍고, 카고 바지를 재활용해 나온 천 조각을 줍고, 임시 붕대 2개 묶음을 제작하고, 배낭을 착용하고, 없는 제작법과 건설을 시도하고, 기다리고, 동쪽/서쪽으로 이동한 뒤 저장합니다. 아이템은 걸어가서 찾지 않고 디버그 메뉴로 아바타 발밑에 만들기 때문에 기록은 매번 같습니다(`tools/client/replay_tutorial.ts`). 짧은 시나리오가 성공했다는 사실은 해당 시나리오의 증거이며 모든 게임 플레이에서 결정적 리플레이가 가능하다는 증명은 아닙니다.

[엔진과 클라이언트](../explanation/engine_clients.md), [MCP](mcp.md), [Tracy 프로파일링](tracy.md)도 참조하세요.

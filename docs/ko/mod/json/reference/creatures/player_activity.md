# 플레이어 활동

플레이어 활동은 턴 단위로 여러 게임 턴에 걸쳐 지속되는 작업입니다. 코드에서 실행되는 활동과 JSON에서 정의된 활동 두 가지 유형이 있습니다.

## 코드에서 정의된 활동

코드에서 정의된 활동은 `activity_type.cpp` 파일에 정의되어 있으며, 다양한 게임 메커니즘에서 사용됩니다. 이러한 활동은 보통 복잡한 상태 관리, 맵 상호작용 또는 다른 게임 시스템과의 통합이 필요한 작업을 위해 사용됩니다.

몇 가지 예시:

- `ACT_CRAFT` - 아이템 제작
- `ACT_MOVE_ITEMS` - 아이템 이동
- `ACT_READ` - 책 읽기
- `ACT_WAIT` - 시간 경과 대기

이러한 활동은 복잡한 로직을 다루기 때문에 JSON이 아닌 C++ 코드로 구현되어 있습니다.

## JSON에서 정의된 활동

간단한 활동의 경우, JSON에서 활동을 정의할 수 있습니다. 이러한 활동은 일반적으로 더 간단한 타이머 기반 작업이거나 특수한 동작 없이 완료되어야 하는 작업입니다.

### 기본 구조

```json
{
  "id": "ACT_EXAMPLE",
  "type": "activity_type",
  "activity_level": "MODERATE_EXERCISE",
  "verb": "examining something",
  "suspendable": false,
  "rooted": true,
  "based_on": "time",
  "no_resume": false,
  "refuel_fires": false,
  "auto_needs": false
}
```

### 필드

| 필드             | 타입    | 필수     | 설명                                                                                                    |
| ---------------- | ------- | -------- | ------------------------------------------------------------------------------------------------------- |
| `id`             | string  | **필수** | 활동의 고유 식별자. 규칙상 `ACT_` 접두사가 붙습니다.                                                    |
| `type`           | string  | **필수** | 반드시 `"activity_type"`이어야 합니다.                                                                  |
| `activity_level` | string  | 선택     | 활동의 신체적 강도. 칼로리 소모와 피로도에 영향을 줍니다. 기본값: `"MODERATE_EXERCISE"`.                |
| `verb`           | string  | 선택     | 플레이어가 활동을 수행할 때 표시되는 동작 (예: "examining", "crafting", "reading").                     |
| `suspendable`    | boolean | 선택     | `true`이면 활동을 일시 중지하고 나중에 재개할 수 있습니다. 기본값: `false`.                             |
| `rooted`         | boolean | 선택     | `true`이면 활동 중 플레이어가 이동할 수 없습니다. 기본값: `true`.                                       |
| `based_on`       | string  | 선택     | 활동 진행률 계산 방식. `"time"` (실시간 틱 기반) 또는 `"speed"` (플레이어 속도 기반). 기본값: `"time"`. |
| `no_resume`      | boolean | 선택     | `true`이면 활동이 중단되면 자동으로 재개되지 않습니다. 기본값: `false`.                                 |
| `refuel_fires`   | boolean | 선택     | `true`이면 이 활동은 근처의 캠프파이어에 자동으로 연료를 공급합니다. 기본값: `false`.                   |
| `auto_needs`     | boolean | 선택     | `true`이면 이 활동은 배고픔, 갈증 등으로 인한 요구사항 경고에 자동으로 중단됩니다. 기본값: `false`.     |

### 활동 레벨

`activity_level` 필드는 활동의 신체적 강도를 정의하며, 다양한 비율로 칼로리 소모와 피로도 축적에 영향을 줍니다:

- `"NO_EXERCISE"` - 수면, 수면하며 대기하기
- `"LIGHT_EXERCISE"` - 독서, 제작, 차량 작업
- `"MODERATE_EXERCISE"` - 걷기 속도의 활동 (기본값)
- `"BRISK_EXERCISE"` - 달리기, 전투
- `"ACTIVE_EXERCISE"` - 수영, 등반
- `"EXTRA_EXERCISE"` - 극도로 격렬한 활동

### 활동 계속 가능 여부

`suspendable` 필드는 활동을 일시 중지하고 나중에 재개할 수 있는지 제어합니다. 다음은 각 설정을 사용해야 하는 경우입니다:

- **`suspendable: false`** - 중단 없이 완료해야 하는 활동 (예: 책 페이지 읽기, 시체 해체 완료)
- **`suspendable: true`** - 진행 상황을 잃지 않고 중단하고 재개할 수 있는 활동 (예: 긴 제작 프로젝트, 아이템 이동)

### 기반 타입

`based_on` 필드는 활동 시간이 진행되는 방식을 제어합니다:

- **`"time"`** (기본값) - 실시간 틱에 따라 진행됩니다. 플레이어 속도나 능력과 무관합니다.
- **`"speed"`** - 플레이어의 이동 속도에 따라 조정됩니다. 속도가 빠른 캐릭터는 더 빨리 완료하고 느린 캐릭터는 더 오래 걸립니다.

대부분의 활동은 `"time"`을 사용해야 합니다. 이동 능력이 완료 시간에 영향을 주어야 하는 활동에만 `"speed"`를 사용하세요.

### 예시: 간단한 검사 활동

```json
{
  "id": "ACT_EXAMINE",
  "type": "activity_type",
  "activity_level": "LIGHT_EXERCISE",
  "verb": "examining",
  "suspendable": false,
  "rooted": true
}
```

이 활동은 플레이어가 무언가를 빠르게 검사하고, 완료될 때까지 이동할 수 없으며, 중단할 수 없습니다.

### 예시: 제작 활동

```json
{
  "id": "ACT_LONGCRAFT",
  "type": "activity_type",
  "activity_level": "LIGHT_EXERCISE",
  "verb": "crafting",
  "suspendable": true,
  "rooted": true,
  "based_on": "time",
  "refuel_fires": true,
  "auto_needs": true
}
```

이 활동은 다음과 같은 특징이 있습니다:

- 일시 중지하고 나중에 재개할 수 있습니다 (`suspendable: true`)
- 플레이어가 이동할 수 없습니다 (`rooted: true`)
- 근처의 캠프파이어에 자동으로 연료를 공급합니다 (`refuel_fires: true`)
- 배고픔이나 갈증이 발생하면 자동으로 일시 중지합니다 (`auto_needs: true`)

### 예시: 이동 기반 활동

```json
{
  "id": "ACT_TRAVELLING",
  "type": "activity_type",
  "activity_level": "MODERATE_EXERCISE",
  "verb": "travelling",
  "suspendable": true,
  "rooted": false,
  "based_on": "speed"
}
```

이 활동은 플레이어가 이동할 수 있으며 (`rooted: false`), 속도를 기반으로 진행되며, 일시 중지하고 재개할 수 있습니다.

## 활동 사용하기

JSON에서 활동을 정의한 후, 코드, 아이템 사용 액션, 또는 다른 게임 메커니즘에서 활동을 참조할 수 있습니다. ID를 사용하여 활동을 참조하세요:

```json
{
  "type": "TOOL",
  "id": "example_tool",
  "use_action": {
    "type": "delayed_action",
    "activity": "ACT_EXAMPLE",
    "duration": "10 seconds"
  }
}
```

활동은 플레이어가 도구를 활성화하면 시작되고 지정된 시간 동안 실행됩니다.

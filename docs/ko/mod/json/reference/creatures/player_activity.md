# 활동

활동은 중단했다가 (선택적으로) 계속할 수 있는 장기 행동입니다. 따라서 아바타나 NPC는 한 턴보다 오래 걸리는 일을 하는 동안에도 몬스터 출현이나 부상 등의 사건에 반응할 수 있습니다.

## 새 활동 추가하기

1. `player_activities.json`에서 해당 활동의 모든 인스턴스에 적용되는 속성을 정의합니다.

2. `activity_actor.h`에서 상태를 저장하고 새 활동의 턴을 처리할 `activity_actor`의 새 하위 클래스(예: `move_items_activity_actor`)를 만듭니다.

3. `activity_actor.cpp`에서 새 액터에 필요한 `start`, `do_turn`, `finish` 함수와 직렬화 함수를 정의합니다. `activity_actor.cpp` 하단의 `deserialize_functions` 맵에 새 활동 액터의 역직렬화 함수를 추가하는 것을 잊지 마세요. 활동이 취소 또는 중단될 때 복원해야 하는 복잡한 상태를 변경한다면 `canceled` 함수도 정의하세요.

4. 이 활동을 재개할 수 있게 하려면 `activity_actor::can_resume_with_internal`을 `override`합니다.

5. 활동 액터를 생성한 뒤 `player_activity` 생성자에 전달합니다. 새로 생성한 활동은 캐릭터에 할당하고 `Character::assign_activity`로 시작할 수 있습니다.

## Lua 기반 활동

Lua 스크립트는 `game.activity_functions[id]`에 Lua 콜백이 있는 임의의 활동을 할당할 수 있습니다.

```lua
game.activity_functions["MY_ACTIVITY_FINISH"] = function(params)
  -- params.user, params.activity, params.name, params.pos, params.data
end

who:assign_lua_activity({
  type = ActivityTypeId.new("ACT_WASH_SELF"),
  duration = TimeDuration.from_minutes(5),
  on_finish = "MY_ACTIVITY_FINISH",
  on_turn = "MY_ACTIVITY_TURN", -- 선택 사항
  pos = target_pos,
  data = { mode = "example" },
})
```

직렬화 가능한 Lua 상태는 `data`에 넣으세요. `pos`는 할당할 때 버블 좌표로 전달하며, 콜백에서는 절대 맵 칸 좌표로 보고됩니다.

## JSON 속성

- verb: 활동 중단 여부를 묻는 질의와 활동을 설명하는 문자열에 쓸 설명 용어입니다. 예: `"verb": "mining"` 또는 `"verb": { "ctxt": "instrument", "str": "playing" }`.

- suspendable (true): true이면 처음부터 다시 시작하지 않고 활동을 계속할 수 있습니다. 이는 `can_resume_with()`가 true를 반환할 때만 가능합니다.

- rooted (false): true이면 활동 중 반동이 줄어들고 식물 변이체는 땅에 뿌리를 내립니다. 몇 분 이상 걸리고 발을 움직이지 않고도 항상 수행할 수 있는 활동에는 true여야 합니다.

- special (false): 다른 활동과 달리 비관습적인 로직을 가질 것으로 예상되는 특수 활동으로 간주합니다.

- complex_moves(false):
  - false이면 활동에 속도 계산이 없으며 매 턴 100 이동력을 소모합니다. JSON에서는 `complex_moves` 블록이 없을 때 지정됩니다.
  - true이면 여러 요소에 따른 복잡한 속도/이동력 계산을 수행합니다.
    - max_assistants(0): 도움을 줄 수 있는 다른 생물의 최대 수(범위: 0~32).
    - bench(false): 작업대를 사용해 활동을 수행할 수 있는지 여부.
    - light(false): 현재 광량이 활동 속도에 영향을 주는지 여부.
    - speed(false): 생물의 속도가 활동 속도에 영향을 주는지 여부.
    - skills: `skill_name: modifier` 쌍으로 제공된 기술이 활동 속도에 영향을 줍니다. 활동에 기술 기반 수정이 적용됨을 명시하고 싶지만 제작이나 건설처럼 실제 필요 기술은 진행 중에 결정해야 한다면 `"skills": true`를 사용합니다.
      - `"skills": true`
      - `"skills": ["fabrication", 5]`
    - stats: `stat_name: modifier` 쌍으로 제공된 능력치가 활동 속도에 영향을 줍니다. 활동에 능력치 기반 수정이 적용됨을 명시하고 싶지만 제작이나 건설처럼 실제 필요 능력치는 진행 중에 결정해야 한다면 `"stats": true`를 사용합니다.
      - `"stats": true`
      - `"stats": ["DEX", 5]`
    - qualities: `q_name: modifier` 쌍으로 제공된 품질이 활동 속도에 영향을 줍니다. 활동에 품질 기반 수정이 적용됨을 명시하고 싶지만 제작이나 건설처럼 실제 필요 품질은 진행 중에 결정해야 한다면 `"qualities": true`를 사용합니다.
      - `"qualities": true`
      - `"qualities": ["CUT_FINE", 5]`
    - morale(false): 생물의 현재 사기 수준이 활동 속도에 영향을 주는지 여부.

    전체 블록 예시:
    ```json
    "complex_moves": {
    "max_assistants": 2,
    "bench": true,
    "light": true,
    "speed": true,
    "stats": true,
    "skills": [ ], - // `"skills": true`와 같음
    "qualities": [ ["CUT_FINE", "5"] ],
    "morale": true
    }
    ```

- morale_blocked(false): 생물의 사기 수준이 특정 수준보다 낮으면 활동을 수행하지 않습니다.

- verbose_tooltip(true): 많은 정보를 표시하는 확장된 진행 창을 사용합니다.

- no_resume (false): 활동을 재개하는 대신 항상 처음부터 다시 시작해야 합니다.

- multi_activity(false): 더 이상 작업할 수 없을 때까지 활동을 반복합니다. NPC와 아바타의 구역 활동에 사용됩니다.

- refuel_fires( false ): true이면 장기 활동 중 캐릭터가 자동으로 불에 연료를 보충합니다.

- auto_needs( false ): true이면 장기 활동 중 캐릭터가 특정 자동 소비 구역에서 자동으로 먹고 마십니다.

- rest_amount( 0.0 ): HP 회복과 관련하여 활동이 얼마나 휴식을 제공하는지 나타냅니다. `rest_amount`가 0.2이면 활동을 수행하는 동안 수면 시 HP 회복량의 20%를 제공합니다.

## 종료

활동은 여러 방식으로 끝낼 수 있습니다.

1. `player_activity::set_to_null()` 호출

   활동이 일찍 끝났거나 데이터 손상, 아이템 소실 등의 문제가 발생했을 때 호출할 수 있습니다. 활동은 백로그에 넣지 않습니다.

2. `moves_left` <= 0

   이 조건이 참이 되면 종료 함수가 있다면 호출됩니다. 종료 함수는 `set_to_null()`을 호출해야 합니다. 종료 함수가 없으면 `set_to_null()`이 자동으로 호출됩니다(`activity_actor::do_turn`에서).

3. `progress.complete()`

   기본적으로 `moves_left` <= 0과 같지만, 추가 검사를 수행하고 진행 시스템을 사용합니다.

4. `Character::cancel_activity`

   활동을 취소하면 `activity_actor::finish` 함수가 실행되지 않으므로 활동은 결과를 만들지 않습니다. 대신 `activity_actor::canceled`가 호출됩니다. 활동을 중단할 수 있으면 사본을 `Character::backlog`에 기록합니다.

## 진행도

`progress_counter`는 활동 진행도를 추적하는 데 특화된 클래스입니다.

- targets: 처리할 것으로 예상되는 대상 큐입니다. 대상 이름, `moves_total`, 대상의 `moves_left`를 저장합니다.

- moves_total (0): 활동, 즉 모든 작업을 완료하는 데 필요한 총 이동력입니다.

- moves_left (): 활동, 즉 모든 작업이 완료되기 전까지 남은 이동력입니다.

- idx (1): 현재 처리 중인 작업의 1부터 시작하는 인덱스입니다.

- total_tasks (0): 완료된 작업과 큐에 있는 작업을 합한 총 작업 수입니다.

## 참고 사항

캐릭터가 활동을 수행하는 동안 매 턴 `activity_actor::do_turn`이 호출됩니다. JSON 속성에 따라 이 함수는 몇 가지 작업을 수행합니다. 또한 `do_turn` 함수를 호출하며, `moves_left`가 양수가 아니면 종료 함수를 호출합니다.

일부 활동(예: MP3 플레이어로 음악 재생)은 최종 결과가 없고, 대신 매 턴 어떤 작업을 합니다(MP3 플레이어로 음악을 재생하면 배터리가 줄고 사기 보너스를 얻음).

활동이 실행 중이거나 끝났을 때 어떤 정보(예: 작업할 _장소_, 사용할 _아이템_ 등)가 필요하다면 활동 액터에 데이터 멤버와 그 데이터를 위한 직렬화 함수를 추가하세요. 그러면 저장/불러오기 주기를 거쳐도 활동을 유지할 수 있습니다.

활동은 NPC가 수행할 수도 있으므로 좌표를 저장할 때 주의하세요. NPC가 수행한다면 좌표는 아바타 위치를 기준으로 하는 로컬 좌표가 아니라 절대 좌표여야 합니다.

### `activity_actor::start`

이 함수는 활동이 캐릭터에 할당될 때 정확히 한 번 호출됩니다. 시간 또는 속도 기반 활동의 경우 `player_activity::moves_left`와 `player_activity::moves_total`을 설정하는 데 유용합니다.

### `activity_actor::do_turn`

무한 루프를 방지하려면 다음 중 하나를 반드시 충족하세요.

- `do_turn`에서 `player_activity::progress.moves_left`를 줄입니다.

- `do_turn`에서 활동을 멈춥니다(위의 '종료' 참조).

예를 들어 `move_items_activity_actor::do_turn`은 캐릭터의 현재 이동력으로 가능한 한 많은 아이템을 옮기거나, 대상 아이템이 남아 있지 않으면 활동을 종료합니다.

### `activity_actor::finish`

이 함수는 활동이 완료되었을 때(`moves_left` <= 0) 호출됩니다. 반드시 `player_activity::set_to_null()`을 호출하거나 새 활동을 할당해야 합니다. 이미 끝난 활동을 다시 시작할 수 있도록 `Character::backlog`에 활동 사본을 만들 수 있으므로, 완료된 활동에 `Character::cancel_activity`를 호출해서는 안 됩니다.

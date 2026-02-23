# 미션 생성하기

NPC는 플레이어에게 미션을 할당할 수 있습니다. 이를 위한 상당히 일반적인 구조가 있습니다:

```json
{
  "id": "MISSION_GET_BLACK_BOX_TRANSCRIPT",
  "type": "mission_definition",
  "name": "Retrieve Black Box Transcript",
  "description": "Decrypt the contents of the black box using a terminal from a nearby lab.",
  "goal": "MGOAL_FIND_ITEM",
  "difficulty": 2,
  "value": 150000,
  "item": "black_box_transcript",
  "start": {
    "effect": { "u_buy_item": "black_box" },
    "assign_mission_target": { "om_terrain": "lab", "reveal_radius": 3 }
  },
  "origins": ["ORIGIN_SECONDARY"],
  "followup": "MISSION_EXPLORE_SARCOPHAGUS",
  "dialogue": {
    "describe": "With the black box in hand, we need to find a lab.",
    "offer": "Thanks to your searching we've got the black box but now we need to have a look'n-side her.  Now, most buildings don't have power anymore but there are a few that might be of use.  Have you ever seen one of those science labs that have popped up in the middle of nowhere?  Them suckers have a glowing terminal out front so I know they have power somewhere inside'em.  If you can get inside and find a computer lab that still works you ought to be able to find out what's in the black box.",
    "accepted": "Fuck ya, America!",
    "rejected": "Do you have any better ideas?",
    "advice": "When I was play'n with the terminal for the one I ran into it kept asking for an ID card.  Finding one would be the first order of business.",
    "inquire": "How 'bout that black box?",
    "success": "America, fuck ya!  I was in the guard a few years back so I'm confident I can make heads-or-tails of these transmissions.",
    "success_lie": "What?!  I out'ta whip you're ass.",
    "failure": "Damn, I maybe we can find an egg-head to crack the terminal."
  }
}
```

### type

항상 존재해야 하며 항상 "mission_definition"이어야 합니다.

### id

미션 id는 필수이지만, 새 미션의 경우 임의로 지정할 수 있습니다. 관례적으로 "MISSION"으로 시작하고 상당히 설명적인 이름을 사용합니다.

### name

이름도 필수이며, 'm'issions 메뉴에서 사용자에게 표시됩니다.

### description

필수는 아니지만, 미션에 대한 모든 관련 정보를 요약하는 것이 강력히 권장됩니다.
"u_buy_item" 타입의 미션 종료 효과를 참조할 수 있지만, 플레이어에게 비용이 발생하지 않는 경우에만 가능합니다. 아래 예제를 참조하세요:

```json
"id": "MISSION_EXAMPLE_TOKENS",
"type": "mission_definition",
"name": "Murder Money",
"description": "Whack the target in exchange for <reward_item:FMCNote> c-notes and <reward_item:cig> cigarettes.",
"goal": "MGOAL_ASSASSINATE",
"end": {
  "effect": [
    { "u_buy_item": "FMCNote", "count": 999 },
    { "u_buy_item": "cig", "count": 666 } ]
}
```

이 시스템은 향후 다른 미션 매개변수와 효과를 참조할 수 있도록 확장될 수 있습니다.

### goal

반드시 포함되어야 하며, 다음 문자열 중 하나여야 합니다:

| goal 문자열               | 목표 조건                                              |
| ------------------------- | ------------------------------------------------------ |
| `MGOAL_GO_TO`             | 특정 오버맵 타일에 도달                                |
| `MGOAL_GO_TO_TYPE`        | 지정된 오버맵 타일 유형의 인스턴스에 도달              |
| `MGOAL_COMPUTER_TOGGLE`   | 올바른 터미널을 활성화하면 미션 완료                   |
| `MGOAL_FIND_ITEM`         | 주어진 유형의 아이템 1개 이상 찾기                     |
| `MGOAL_FIND_ITEM_GROUP`   | 주어진 item_group의 아이템 1개 이상 찾기               |
| `MGOAL_FIND_ANY_ITEM`     | 이 미션에 태그된 주어진 유형의 아이템 1개 이상 찾기    |
| `MGOAL_FIND_MONSTER`      | 우호적인 몬스터를 찾아서 회수                          |
| `MGOAL_FIND_NPC`          | 특정 NPC 찾기                                          |
| `MGOAL_TALK_TO_NPC`       | 특정 NPC와 대화하기                                    |
| `MGOAL_RECRUIT_NPC`       | 특정 NPC 모집하기                                      |
| `MGOAL_RECRUIT_NPC_CLASS` | 특정 클래스의 NPC 모집하기                             |
| `MGOAL_ASSASSINATE`       | 특정 NPC 처치하기                                      |
| `MGOAL_KILL_MONSTER`      | 특정 적대적 몬스터 처치하기                            |
| `MGOAL_KILL_MONSTER_TYPE` | 특정 몬스터 유형을 일정 수 처치하기                    |
| `MGOAL_KILL_MONSTER_SPEC` | 특정 종의 몬스터를 일정 수 처치하기                    |
| `MGOAL_KILL_MONSTERS`     | 미션 시작 시 생성된 특정 미션 태그 몬스터들 처치하기   |
| `MGOAL_CONDITION`         | 동적으로 생성된 조건을 만족하고 미션 제공자와 대화하기 |

### monster_species

"MGOAL_KILL_MONSTER_SPEC"의 경우, 대상 몬스터 종을 설정합니다.

### monster_type

"MGOAL_KILL_MONSTER_TYPE"의 경우, 대상 몬스터 유형을 설정합니다.

### monster_kill_goal

"MGOAL_KILL_MONSTER_SPEC"와 "MGOAL_KILL_MONSTER_TYPE"의 경우, 미션 완료를 위해 플레이어의 현재 처치 수 위에 추가로 처치해야 할 몬스터 수를 설정합니다.

### goal_condition

"MGOAL_CONDITION"의 경우, 미션이 완료된 것으로 간주되기 위해 만족되어야 하는 조건을 정의합니다. 조건은 [NPCs.md](./NPCs)에서 더 자세히 설명되며, 여기서 정확히 같은 방식으로 사용됩니다.

### dialogue

이것은 문자열 사전입니다. NPC는 플레이어가 미션에 대해 문의하거나 완료를 보고할 때 이 정확한 문자열들을 말합니다. 미션에서 사용되지 않을 수 있더라도 이 모든 문자열은 필수입니다.

| 문자열 ID     | 용도                                                                       |
| ------------- | -------------------------------------------------------------------------- |
| `describe`    | 미션에 대한 NPC의 전반적인 설명                                            |
| `offer`       | 플레이어가 고려를 위해 해당 미션을 선택했을 때 제공되는 미션의 세부 사항   |
| `accepted`    | 플레이어가 미션을 수락할 경우 NPC의 반응                                   |
| `rejected`    | 플레이어가 미션을 거부할 경우 NPC의 반응                                   |
| `advice`      | 플레이어가 미션 완료 방법에 대한 조언을 요청할 경우, 이 내용을 듣게 됩니다 |
| `inquire`     | NPC가 플레이어에게 미션이 어떻게 진행되고 있는지 물어볼 때 사용됩니다      |
| `success`     | 미션 성공 보고에 대한 NPC의 반응                                           |
| `success_lie` | NPC가 플레이어가 미션 성공에 대해 거짓말하는 것을 잡았을 때의 반응         |
| `failure`     | 플레이어가 실패한 미션을 보고할 경우 NPC의 반응                            |

### start

선택적 필드입니다. 존재하고 문자열인 경우, mission * 를 받아 미션에 대한 시작 코드를 수행하는 mission_start:: 내의 함수 이름이어야 합니다. 이를 통해 표준 미션 유형 이외의 미션을 실행할 수 있습니다. "MGOAL_COMPUTER_TOGGLE"을 사용하는 미션을 설정하려면 현재 하드코딩된 함수가 필요합니다.

또는, 존재하는 경우 아래에 설명된 대로 객체일 수 있습니다.

### start / end / fail 효과

이러한 선택적 필드 중 하나라도 존재하는 경우, 다음 필드를 포함하는 객체일 수 있습니다:

#### effect

이것은 [NPCs.md](./NPCs)에서 정확히 정의된 대로 효과 배열이며, 효과의 모든 값을 사용할 수 있습니다. 모든 경우에, 관련된 NPC는 퀘스트 제공자입니다.

#### reveal_om_ter

이것은 문자열 또는 문자열 목록일 수 있으며, 모두 오버맵 지형 ID여야 합니다. 해당 ID - 또는 목록에서 무작위로 선택된 ID 중 하나 - 를 가진 무작위로 선택된 오버맵 지형 타일이 드러나며, 맵 타일로 가는 도로 경로도 드러날 확률이 3분의 1입니다.

#### assign_mission_target

`assign_mission_target` 객체는 특정 오버맵 지형을 찾거나 (필요한 경우 생성하여) 미션 대상으로 지정하기 위한 기준을 지정합니다. 매개변수를 통해 선택 방법과 일부 효과(주변 지역 드러내기 등)가 이후에 적용되는 방법을 제어할 수 있습니다. `om_terrain`이 유일한 필수 필드입니다.

| 식별자                                     | 설명                                                                                                 |
| ------------------------------------------ | ---------------------------------------------------------------------------------------------------- |
| `om_terrain`                               | 대상으로 선택될 오버맵 지형의 ID. 필수.                                                              |
| `om_terrain_match_type`                    | `om_terrain`과 함께 사용할 매칭 규칙. 기본값은 TYPE. 자세한 내용은 아래 참조.                        |
| `om_special`                               | 오버맵 지형을 포함하는 오버맵 스페셜의 ID.                                                           |
| `om_terrain_replace`                       | `om_terrain`을 찾을 수 없는 경우 찾아서 교체할 오버맵 지형의 ID.                                     |
| `reveal_radius`                            | 드러낼 오버맵 지형 좌표의 반경.                                                                      |
| `must_see`                                 | true인 경우, `om_terrain`은 이미 봤어야 합니다.                                                      |
| `cant_see`                                 | true인 경우, `om_terrain`은 아직 보지 않았어야 합니다.                                               |
| `random`                                   | true인 경우, 무작위로 매칭되는 `om_terrain`을 사용합니다. false인 경우, 가장 가까운 것을 사용합니다. |
| `search_range`                             | 매칭되는 `om_terrain`을 찾을 오버맵 지형 좌표의 범위.                                                |
| `min_distance`                             | 오버맵 지형 좌표의 범위. 이 범위 내의 `om_terrain` 인스턴스는 무시됩니다.                            |
| `origin_npc`                               | 플레이어가 아닌 NPC의 현재 위치에서 검색을 시작합니다.                                               |
| `z`                                        | 지정된 경우, 검색 시 플레이어나 NPC의 z 대신 사용됩니다.                                             |
| `offset_x`,<br\>`offset_y`,<br\>`offset_z` | `om_terrain`을 찾거나 생성한 후, 오버맵 지형 좌표의 오프셋만큼 미션 대상 지형을 오프셋합니다.        |

**예제**

```json
{
  "assign_mission_target": {
    "om_terrain": "necropolis_c_44",
    "om_special": "Necropolis",
    "reveal_radius": 1,
    "must_see": false,
    "random": true,
    "search_range": 180,
    "z": -2
  }
}
```

`om_terrain`이 오버맵 스페셜의 일부인 경우, `om_special` 값도 지정하는 것이 필수적입니다--그렇지 않으면, 게임은 전체 스페셜을 생성하는 방법을 알 수 없습니다.

`om_terrain_match_type`은 지정되지 않으면 기본적으로 TYPE이며, 다음과 같은 가능한 값들이 있습니다:

- `EXACT` - 제공된 문자열은 선형 지형 유형의 선형 방향 접미사 또는 회전된 지형 유형의 회전 접미사를 포함하여 오버맵 지형 ID와 완전히 일치해야 합니다.

- `TYPE` - 제공된 문자열은 오버맵 지형 ID의 기본 유형 ID와 완전히 일치해야 하며, 이는 회전 및 선형 지형 유형의 접미사가 무시된다는 것을 의미합니다.

- `PREFIX` - 제공된 문자열은 오버맵 지형 ID의 완전한 접두사(추가 부분은 언더스코어로 구분됨)여야 합니다. 예를 들어, "forest"는 "forest" 또는 "forest_thick"와 일치하지만 "forestcabin"과는 일치하지 않습니다.

- `CONTAINS` - 제공된 문자열은 오버맵 지형 ID 내에 포함되어야 하지만, 시작, 끝 또는 중간에 나타날 수 있으며 언더스코어 구분에 대한 규칙이 없습니다.

`om_special`을 배치해야 하는 경우, 오버맵 스페셜 정의에서 정의된 것과 동일한 배치 규칙을 따르며, 허용된 지형, 도시로부터의 거리, 도로 연결 등을 존중합니다. 따라서, 규칙이 더 제한적일수록, 이 배치가 성공할 가능성이 낮아집니다(이미 생성된 스페셜과 공간을 경쟁하기 때문입니다).

`om_terrain_replace`는 `om_terrain`이 오버맵 스페셜의 일부가 아닌 경우에만 관련이 있습니다. 이 값은 `om_terrain`을 찾을 수 없는 경우 사용되며, 그 다음 `om_terrain` 값으로 교체될 대체 대상으로 사용됩니다.

`must_see`가 true로 설정되면, 게임은 지형을 찾을 수 없는 경우 지형을 생성하지 않습니다. 이는 플레이어가 이미 방문한 지역에 새로운 지형이 마법처럼 나타나는 것을 피하려는 것이며, 이는 그 결과입니다.

`reveal_radius`, `min_distance`, `search_range`는 모두 오버맵 지형 좌표(오버맵은 현재 180 x 180 OMT 단위)입니다. 검색은 `origin_npc`가 설정되지 않은 경우 플레이어를 중심으로 하며, 설정된 경우 NPC를 중심으로 합니다. 현재 두 사이에는 거의 차이가 없지만, 라디오를 통해 미션을 할당하는 것이 가능합니다. 검색은 기존 오버맵을 우선시하며(지형을 기존 오버맵에서 찾을 수 없는 경우에만 새 오버맵을 생성합니다), 플레이어의 현재 z-레벨에서만 실행됩니다. `z` 속성을 사용하여 이를 재정의하고 플레이어와 다른 z-레벨에서 검색할 수 있습니다--이것은 상대적이 아닌 절대값입니다.

`offset_x`, `offset_y`, `offset_z`는 미션 대상의 최종 위치를 해당 값만큼 변경합니다. 이는 미션 대상의 오버맵 지형 유형을 `om_terrain`에서 벗어나게 변경할 수 있습니다.

#### update_mapgen

`update_mapgen` 객체 또는 배열은 미션별 몬스터, NPC, 컴퓨터 또는 아이템을 추가하기 위해 기존 오버맵 타일("assign_mission_target"에 의해 생성된 것 포함)을 수정하는 방법을 제공합니다.

배열로서, `update_mapgen`은 두 개 이상의 `update_mapgen` 객체로 구성됩니다.

객체로서, `update_mapgen`은 유효한 JSON 맵젠 객체를 포함합니다. 객체는 "assign_mission_target"의 미션 대상 지형 또는 선택적으로 `om_terrain` 및 `om_special` 필드에 의해 지정된 가장 가까운 오버맵 지형에 배치됩니다. "mapgen_update_id"가 지정된 경우, 일치하는 "mapgen_update_id"를 가진 "mapge_update" 객체가 실행됩니다.

JSON 맵젠 및 `update_mapgen`에 대한 자세한 내용은 doc/MAPGEN.md를 참조하세요.

`update_mapgen`을 사용하여 배치된 NPC, 몬스터 또는 컴퓨터는 `update_mapgen`의 `place` 객체에 `target` 불리언이 `true`로 설정되어 있으면 미션의 대상이 됩니다.

## NPC 대화에 새 미션 추가하기

NPC에 미션을 할당하려면, 첫 번째 단계는 해당 NPC의 정의를 찾는 것입니다. 고유 NPC의 경우 이는 일반적으로 npc의 JSON 파일 상단에 있으며 다음과 같이 보입니다:

```json
{
  "type": "npc",
  "id": "refugee_beggar2",
  "//": "Schizophrenic beggar in the refugee center.",
  "name_unique": "Dino Dave",
  "gender": "male",
  "name_suffix": "beggar",
  "class": "NC_BEGGAR_2",
  "attitude": 0,
  "mission": 7,
  "chat": "TALK_REFUGEE_BEGGAR_2",
  "faction": "lobby_beggars"
},
```

NPC의 시작 미션을 정의하는 새 줄을 추가합니다, 예:

```
"mission_offered": "MISSION_BEGGAR_2_BOX_SMALL"
```

미션이 있는 모든 NPC는 플레이어가 NPC의 첫 번째 미션을 시작할 수 있도록 TALK_MISSION_LIST로 이어지는 대화 옵션이 있어야 하며, 다음 중 하나가 필요합니다:

- data/json/npcs/TALK_COMMON_MISSION.json의 첫 번째 talk_topic에 있는 일반 미션 응답 ID 목록에 해당 talk_topic ID 중 하나를 추가하거나,
- TALK_MISSION_INQUIRE 및 TALK_MISSION_LIST_ASSIGNED로 이어지는 응답이 있는 유사한 talk_topic을 가집니다.

이 두 옵션 중 하나를 사용하면 플레이어가 NPC와 일반 미션 관리 대화를 할 수 있습니다.

이것은 커스텀 미션 문의가 어떻게 나타날 수 있는지에 대한 예입니다. 이것은 플레이어가 이미 미션을 할당받은 경우에만 NPC의 대화 옵션에 나타납니다.

```json
{
    "type": "talk_topic",
    "//": "Generic responses for Old Guard Necropolis NPCs that can have missions",
    "id": [ "TALK_OLD_GUARD_NEC_CPT", "TALK_OLD_GUARD_NEC_COMMO" ],
    "responses": [
      {
        "text": "About the mission...",
        "topic": "TALK_MISSION_INQUIRE",
        "condition": { "and": [ "has_assigned_mission", { "u_is_wearing": "badge_marshal" } ] }
      },
      {
        "text": "About one of those missions...",
        "topic": "TALK_MISSION_LIST_ASSIGNED",
        "condition": { "and": [ "has_many_assigned_missions", { "u_is_wearing": "badge_marshal" } ] }
      }
    ]
},
```

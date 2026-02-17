# 날씨 타입

날씨 타입은 발생할 수 있는 조건(온도, 습도, 기압, 풍력, 시간 등)과 게임 월드와 현실 버블에 미치는 영향을 지정합니다.

날씨 타입을 선택할 때 게임은 지역 설정에 정의된 목록을 검토하고 현재 조건에서 적격으로 간주되는 마지막 항목을 선택합니다. 적격 항목이 없으면 잘못된 날씨 타입 `"none"`이 사용됩니다.

### 필드

| 식별자            | 설명                                                                                                              |
| ----------------- | ----------------------------------------------------------------------------------------------------------------- |
| id                | (_필수_) 고유 ID. 필요한 경우 밑줄을 사용하여 하나의 연속된 단어여야 합니다.                                      |
| name              | (_필수_) 표시되는 게임 내 이름.                                                                                   |
| color             | (_필수_) 게임 내 이름의 색상.                                                                                     |
| glyph             | (_필수_) 오버맵에 사용되는 기호.                                                                                  |
| map_color         | (_필수_) 오버맵 기호의 색상.                                                                                      |
| ranged_penalty    | (_필수_) 원거리 공격에 대한 페널티.                                                                               |
| sight_penalty     | (_필수_) 시야 페널티, 즉 타일 투명도에 대한 승수.                                                                 |
| light_modifier    | (_필수_) 주변 조명에 대한 평면 보너스.                                                                            |
| sound_attn        | (_필수_) 소리 감쇠(볼륨에 대한 평면 감소).                                                                        |
| dangerous         | (_필수_) true이면 활동 중단을 요청합니다.                                                                         |
| precip            | (_필수_) 관련 강수량. 유효한 값: `none`, `very_light`, `light`, `heavy`.                                          |
| rains             | (_필수_) 해당 강수가 비로 내리는지 여부.                                                                          |
| acidic            | (_선택적_) 해당 강수가 산성인지 여부.                                                                             |
| sound_category    | (_선택적_) 재생할 음향 효과. 유효한 값: `silent`, `drizzle`, `rainy`, `thunder`, `flurries`, `snowstorm`, `snow`. |
| sun_intensity     | (_필수_) 햇빛 강도. 유효한 값: `none`, `light`, `normal`, `high`. Normal과 high는 "직사광선"으로 간주됩니다.      |
| weather_animation | (_선택적_) 현실 버블의 날씨 애니메이션. [상세 정보](#weather_animation)                                           |
| effects           | (_선택적_) 날씨가 일으키는 효과에 대한 `[string, int]` 쌍 배열. [상세 정보](#effects)                             |
| requirements      | (_선택적_) 이 날씨 타입이 선택될 수 있는 조건. [상세 정보](#requirements)                                         |

### weather_animation

모든 멤버는 필수입니다.

| 식별자 | 설명                                      |
| ------ | ----------------------------------------- |
| factor | 표시 밀도: 0은 없음, 1은 화면을 가립니다. |
| glyph  | ASCII 모드에서 사용할 기호.               |
| color  | 기호 색상.                                |
| tile   | TILES 모드에서 사용할 그래픽 타일.        |

### effects

여기서 `int`는 효과 강도입니다.

| 식별자     | 설명                                                           |
| ---------- | -------------------------------------------------------------- |
| wet        | 플레이어를 `int` 양만큼 젖게 함                                |
| thunder    | `int` 중 1의 확률로 천둥 소리                                  |
| lightning  | `int` 중 1의 확률로 소리와 메시지, 그리고 전기장 과충전 가능성 |
| light_acid | 방수가 아니면 고통을 일으킴                                    |
| acid_rain  | 방수가 아니면 더 많은 고통을 일으킴                            |
| morale     | 플레이어가 사기 효과를 느끼게 함                               |
| effect     | 플레이어가 상태 효과에 걸리게 함                               |

사기 예제:

```json
{
  "name": "morale",
  "intensity": 3, // 이 효과는 X 턴마다 적용됩니다.
  "bonus": 2, // 사기가 제공하는 보너스.
  "bonus_max": 60, // 사기가 플레이어에게 영향을 줄 수 있는 최대 양.
  "duration": "180 s", // 사기가 지속되는 시간.
  "decay_start": "60 s", // 효과가 지속 시간을 카운트다운하기 시작하기 전의 시간.
  "morale_id_str": "morale_weather_rainbow", // 플레이어에게 적용되는 사기의 ID.
  "morale_msg": "You stare in awe at the rainbow.", // 플레이어가 영향을 받을 때 채팅에 표시할 메시지.
  "morale_msg_frequency": 8, // 플레이어가 영향을 받을 때마다 이 메시지를 표시할 확률.
  "message_type": 0 // 표시할 메시지 타입: good, bad, mixed 등.
}
```

효과 예제:

```json
{
  "name": "effect",
  "intensity": 3, // 이 효과는 X 턴마다 적용됩니다.
  "duration": "30 s", // 효과가 지속되는 시간.
  "effect_id_str": "emp",
  "effect_intensity": 0, // 적용되는 효과의 강도.
  "precipitation_name": "brain waves", // "The <PRECIPITATION> is blocked by your umbrella!" 타입 메시지가 표시될 때 표시할 강수 이름.
  "ignore_armor": true, // 모든 보호를 무시합니다.
  "bodypart_string": "head", // 효과를 적용할 신체 부위.
  "effect_msg": "You feel an odd wave-like sensation pass through your head.", // 플레이어가 영향을 받을 때 채팅에 표시할 메시지.
  "effect_msg_frequency": 16, // 플레이어가 영향을 받을 때마다 이 메시지를 표시할 확률.
  "effect_msg_blocked_frequency": 32, // 플레이어가 옷으로 효과를 차단할 때마다 이 메시지를 표시할 확률.
  "message_type": 2, // 표시할 메시지 타입: good, bad, mixed 등.
  "clothing_protection": 0, // 강수를 차단할 X 중 1의 확률.
  "umbrella_protection": 0 // 강수를 차단할 X 중 1의 확률.
}
```

### requirements

모든 멤버는 선택 사항입니다.

| 식별자                | 설명                                                                                                                                                                                                                     |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| pressure_min          | 최소 기압                                                                                                                                                                                                                |
| pressure_max          | 최대 기압                                                                                                                                                                                                                |
| humidity_min          | 최소 습도                                                                                                                                                                                                                |
| humidity_max          | 최대 습도                                                                                                                                                                                                                |
| temperature_min       | 최소 온도                                                                                                                                                                                                                |
| temperature_max       | 최대 온도                                                                                                                                                                                                                |
| windpower_min         | 최소 풍력                                                                                                                                                                                                                |
| windpower_max         | 최대 풍력                                                                                                                                                                                                                |
| humidity_and_pressure | 기압과 습도 요구 사항이 있는 경우 둘 다 필요한지 아니면 하나만 필요한지                                                                                                                                                  |
| acidic                | 산성 강수가 필요한지                                                                                                                                                                                                     |
| time                  | 시간대. 유효한 값: day, night, both.                                                                                                                                                                                     |
| required_weathers     | 지정된 타입 중 하나에 대한 조건이 일치하는 경우에만 선택됩니다. 즉, 비는 구름, 가벼운 이슬비 또는 이슬비에 대한 조건이 있는 경우에만 발생할 수 있습니다. 필요한 날씨는 지역 날씨 목록에서 이것보다 "앞에" 있어야 합니다. |

### 예제

```json
{
  "id": "lightning",
  "type": "weather_type",
  "name": "Lightning Storm",
  "color": "c_yellow",
  "map_color": "h_yellow",
  "glyph": "%",
  "ranged_penalty": 4,
  "sight_penalty": 1.25,
  "light_modifier": -45,
  "sound_attn": 8,
  "dangerous": false,
  "precip": "heavy",
  "rains": true,
  "acidic": false,
  "effects": [{ "name": "thunder", "intensity": 50 }, { "name": "lightning", "intensity": 600 }],
  "tiles_animation": "weather_rain_drop",
  "weather_animation": { "factor": 0.04, "color": "c_light_blue", "glyph": "," },
  "sound_category": "thunder",
  "sun_intensity": "none",
  "requirements": { "pressure_max": 990, "required_weathers": ["thunder"] }
}
```

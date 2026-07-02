# 날씨 타입

날씨 타입은 날씨가 발생할 수 있는 조건(온도, 습도, 기압,
풍력, 시간 등) 그리고 그것이 게임 세계와 현실 거품에 어떤 영향을 미치는지.

날씨 타입을 선택할 때 게임은 지역 설정에 정의된 목록을 검토하고 날씨 타입을 선택합니다.
현재 상황에서 적격하다고 간주되는 마지막 항목입니다. 항목이 하나도 없는 경우
적합하지만 잘못된 날씨 타입 `"none"`이(가) 사용됩니다.

### 필드

| 식별자         | 설명                                                                                                                       |
| -------------- | -------------------------------------------------------------------------------------------------------------------------- |
| id             | (_필수_) 고유 ID입니다. 하나의 연속된 단어여야 하며 필요한 경우 밑줄을 사용하세요.                                         |
| name           | (_필수_) 게임 내 이름이 표시됩니다.                                                                                        |
| glyph          | (_필수_) 오버맵에 사용되는 글리프입니다.                                                                                   |
| color          | (_선택 사항_) 게임 내 이름의 색상입니다.                                                                                   |
| map_color      | (_선택 사항_) 오버맵 문자 모양의 색상입니다.                                                                               |
| ranged_penalty | (_필수_) 원거리 공격에 대한 페널티.                                                                                        |
| sight_penalty  | (_필수_) 시각 패널티, 일명 타일 투명도 승수.                                                                               |
| light_modifier | (_필수_) 주변광에 대한 고정 보너스.                                                                                        |
| sound_attn     | (_필수_) 사운드 감쇠(볼륨을 균일하게 감소).                                                                                |
| dangerous      | (_필수_) true인 경우 활동 중단을 묻는 메시지가 표시됩니다.                                                                 |
| precip         | (_필수_) 관련 강수량입니다. 유효한 값은 `none`, `very_light`, `light` 및 `heavy`입니다.                                    |
| rains          | (_필수_) 해당 강수량이 비로 내리는지 여부입니다.                                                                           |
| acidic         | (_선택 사항_) 해당 강수량이 산성인지 여부.                                                                                 |
| sound_category | (_선택적_) 재생할 음향 효과. 유효한 값은 `silent`, `drizzle`, `rainy`, `thunder`, `flurries`, `snowstorm` 및 `snow`입니다. |
| sun_intensity  | (_필수_) 햇빛 강도. 유효한 값은 `none`, `light`, `normal` 및 `high`입니다. 보통과 높음은 "direct sunlight"로 간주됩니다.   |
| animation      | (_선택적_) 현실 버블의 날씨 애니메이션. [세부정보](#weather_animation)                                                     |
| effects        | (_선택적_) `[string, int]` 날씨로 인한 영향에 대한 쌍 배열입니다. [세부정보](#effects)                                     |
| requirements   | (_선택 사항_) 이 날씨 타입을 선택할 수 있는 조건입니다. [세부정보](#requirements)                                          |

<a id="weather_animation"></a>

### 애니메이션

모든 회원은 필수입니다.

| 식별자 | 설명                                                 |
| ------ | ---------------------------------------------------- |
| factor | 디스플레이 밀도: 0은 없음, 1은 화면을 흐리게 합니다. |
| glyph  | ASCII 모드에서 사용할 글리프입니다.                  |
| color  | 글리프 색상.                                         |
| tile   | TILES 모드에서 사용할 그래픽 타일입니다.             |

<a id="effects"></a>

### 효과

#### 범용 필드

| 식별자    | 설명                                                                             |
| --------- | -------------------------------------------------------------------------------- |
| name      | (_필수_) 아래 나열된 효과 중 하나는 일부에 대해 로드된 json을 완전히 변경합니다. |
| intensity | (_필수_) 부여하는 효과의 강도                                                    |

##### 이름

- `morale`: 플레이어의 사기를 높이고 추가로 정의된 필드가 필요합니다.
- `effect`: 플레이어에게 효과를 제공하며 추가로 정의된 필드가 필요합니다.
- `wet`: 플레이어를 젖게 만듭니다. 양은 강도입니다.
- `thunder`: 천둥은 강도 확률이 1로 발생합니다.
- `lightning`: 강도 확률이 1인 번개가 발생합니다.

#### 사기

| 식별자               | 설명                                                                                         |
| -------------------- | -------------------------------------------------------------------------------------------- |
| intensity            | (_필수_) 이 효과는 x턴마다 적용됩니다.                                                       |
| bonus                | (_필수_) 각 스택이 제공하는 사기의 양                                                        |
| max_bonus            | (_필수_) 최대 사기 획득량                                                                    |
| duration             | (_필수_) 사기 효과 지속 시간                                                                 |
| decay_start          | (_필수_) 효과의 강도가 낮아지기 시작할 때까지의 총 지속시간은 이 값에 지속시간이 추가됩니다. |
| morale_id_str        | (_필수_) 사기효과가 적용된 이드                                                              |
| morale_msg           | (_필수_) 사기를 높였을 때 전하는 메시지                                                      |
| morale_msg_frequency | (_필수_) 효과 획득 시 메시지를 표시할 확률(%)                                                |
| message_type         | (_필수_) 메시지 유형입니다. 0 == 좋음, 1 == 나쁨, 2 == 혼합, 3 == 경고, 4 == 정보, 5 == 중립 |

##### 예:

```json
{
  "name": "morale",
  "intensity": 3,
  "bonus": 2,
  "bonus_max": 60,
  "duration": "180 s",
  "decay_start": "60 s",
  "morale_id_str": "morale_weather_rainbow",
  "morale_msg": "You stare in awe at the rainbow.",
  "morale_msg_frequency": 8,
  "message_type": 0
}
```

#### 효과

| 식별자                       | 설명                                                                                                                  |
| ---------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| intensity                    | (_필수_) 이 효과는 x턴마다 적용됩니다.                                                                                |
| duration                     | (_필수_) 효과 지속 시간                                                                                               |
| effect_id_str                | (_필수_) 제공할 효과 ID의 문자열                                                                                      |
| effect_intensity             | (_필수_) 부여하는 효과의 강도                                                                                         |
| bodypart_string              | (_선택 사항_) 효과를 적용할 신체 부위, 기본 전신                                                                      |
| effect_msg                   | (_필수_) 효과 획득 시 주는 메시지                                                                                     |
| effect_msg_frequency         | (_필수_) 효과 획득 시 메시지를 표시할 확률(%)                                                                         |
| effect_msg_blocked_frequency | (_필수_) 효과 방지 시 보호 메시지를 표시할 확률(%)                                                                    |
| message_type                 | (_필수_) 메시지 유형입니다. 0 == 좋음, 1 == 나쁨, 2 == 혼합, 3 == 경고, 4 == 정보, 5 == 중립                          |
| precipitation_name           | (_필수_) 문자열에 표시할 이름 "당신의 우산이 `precipitation_name`으로부터 당신을 보호합니다"                          |
| protection_data              | (_선택적_) 효과로부터 보호할 `check` 플래그 또는 특성(항상 적용되는 `DEFAULT`과 함께) 및 `odds`(x 기회에 하나)의 배열 |

##### 예:

```json
{
  "name": "effect",
  "intensity": 3,
  "duration": "30 s",
  "effect_id_str": "emp",
  "effect_intensity": 0,
  "precipitation_name": "brain waves",
  "bodypart_string": "head",
  "effect_msg": "You feel an odd wave-like sensation pass through your head.",
  "effect_msg_frequency": 16,
  "effect_msg_blocked_frequency": 32,
  "message_type": 2,
  "protection_data": [
    { "check": "RAIN_PROTECT", "odds": 1 },
    { "check": "ACIDPROOF", "odds": 1 },
    { "check": "DEFAULT", "odds": 2 }
  ]
}
```

<a id="requirements"></a>

### 요구 사항

모든 회원은 선택사항입니다.

| 식별자                | 설명                                                                                                                                                                                                  |
| --------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| pressure_min          | 최소 압력                                                                                                                                                                                             |
| pressure_max          | 최대 압력                                                                                                                                                                                             |
| humidity_min          | 최소 습도                                                                                                                                                                                             |
| humidity_max          | 최대 습도                                                                                                                                                                                             |
| temperature_min       | 최소 온도                                                                                                                                                                                             |
| temperature_max       | 최대 온도                                                                                                                                                                                             |
| windpower_min         | 최소 풍력                                                                                                                                                                                             |
| windpower_max         | 최대 풍력                                                                                                                                                                                             |
| humidity_and_pressure | 압력 및 습도 요구 사항이 있는 경우 둘 다 필요합니까, 아니면 하나만 필요합니까?                                                                                                                        |
| acidic                | 산성 침전이 필요한가요?                                                                                                                                                                               |
| time                  | 시간. 유효한 값은 낮, 밤 및 둘 다입니다.                                                                                                                                                              |
| required_weathers     | 지정된 유형 중 하나에 대해 조건이 일치하는 경우에만 선택됩니다. 즉, 비는 구름, 가벼운 이슬비 또는 이슬비 조건이 있는 경우에만 발생할 수 있습니다. 필수 날씨는 지역 날씨 목록의 "before"이어야 합니다. |

### 예

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

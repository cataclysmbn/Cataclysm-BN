# 시나리오

시나리오는 `type` 멤버가 `scenario`로 설정된 JSON 객체로 지정됩니다.

```json
{
    "type": "scenario",
    "id": "schools_out",
    ...
}
```

id 멤버는 시나리오의 고유 id여야 합니다.

다음 속성들이 지원됩니다 (별도 표기가 없으면 필수):

## `description`

(문자열)

게임 내 설명.

## `name`

(문자열 또는 "male"과 "female" 멤버가 있는 객체)

게임 내 이름, 성별 중립적인 단일 문자열이거나 성별별 이름이 있는 객체.
예시:

```json
"name": {
    "male": "Runaway groom",
    "female": "Runaway bride"
}
```

## `points`

(정수)

시나리오의 포인트 비용. 양수 값은 포인트를 소비하고 음수 값은 포인트를 부여합니다.

## `items`

(선택사항, 선택적 멤버 "both", "male", "female"가 있는 객체)

이 시나리오를 선택할 때 플레이어가 시작하는 아이템. 캐릭터의 성별에 따라 다른 아이템을 지정할 수 있습니다.
각 아이템 목록은 아이템 id의 배열이어야 합니다. Id는 여러 번 나타날 수 있으며, 이 경우 아이템이 여러 번 생성됩니다.

예시:

```json
"items": {
    "both": [
        "pants",
        "rock",
        "rock"
    ],
    "male": [ "briefs" ],
    "female": [ "panties" ]
}
```

이것은 플레이어에게 바지, 두 개의 돌, 그리고 (성별에 따라) 팬티 또는 브리프를 줍니다.

모드는 "add:both" / "add:male" / "add:female"과 "remove:both" / "remove:male" / "remove:female"을 통해 기존 시나리오의 목록을 수정할 수 있습니다.

모드 예시:

```json
{
  "type": "scenario",
  "id": "schools_out",
  "edit-mode": "modify",
  "items": {
    "remove:both": ["rock"],
    "add:female": ["2x4"]
  }
}
```

## `surround_groups`

(선택사항, 그룹과 밀도 숫자가 있는 배열)

이것은 시나리오의 `SUR_START` 플래그를 대체하며, 시나리오의 시작 위치를 둘러싼 지역에 생성되는 몬스터 그룹을 지정합니다.

```json
"surround_groups": [ [ "GROUP_BLACK_ROAD", 70.0 ] ],
```

문자열은 주변 지역에 생성될 몬스터 그룹의 ID를 정의하며, 숫자는 생성 밀도입니다.
70.0은 원래 `SUR_START` 동작의 값을 복제합니다.

## `flags`

(선택사항, 문자열 배열)

플래그 목록. TODO: 여기에 플래그를 문서화해야 합니다.

모드는 "add:flags"와 "remove:flags"를 통해 이것을 수정할 수 있습니다.

## `cbms`

(선택사항, 문자열 배열)

캐릭터에 이식된 CBM id 목록.

모드는 "add:CBMs"와 "remove:CBMs"를 통해 이것을 수정할 수 있습니다.

## `traits", "forced_traits", "forbidden_traits`

(선택사항, 문자열 배열)

특성/변이 id 목록. "forbidden_traits"의 특성은 금지되며 캐릭터 생성 중에 선택할 수 없습니다.
"forced_traits"의 특성은 캐릭터에 자동으로 추가됩니다. "traits"의 특성은 시작 특성이 아니더라도 선택할 수 있게 합니다.

모드는 "add:traits" / "add:forced_traits" / "add:forbidden_traits"와
"remove:traits" / "remove:forced_traits" / "remove:forbidden_traits"를 통해 이것을 수정할 수 있습니다.

## `bionics", "forced_bionics", "forbidden_bionics`

(선택사항, 문자열 배열)

특성/변이 id 목록. "forbidden_bionics"의 생체공학은 금지되며 캐릭터 생성 중에 선택할 수 없습니다.
"forced_bionics"의 생체공학은 캐릭터에 자동으로 추가됩니다. "bionics"의 생체공학은 시작 생체공학이 아니더라도 선택할 수 있게 합니다.

모드는 "add:bionics" / "add:forced_bionics" / "add:forbidden_bionics"와
"remove:bionics" / "remove:forced_bionics" / "remove:forbidden_bionics"를 통해 이것을 수정할 수 있습니다.

## `forbids_bionics`

(선택사항, bool)

플레이어가 생체공학 캐릭터 생성 탭을 통해 생체공학을 추가하는 것을 금지합니다.

## `allowed_locs`

(선택사항, 문자열 배열)

이 시나리오를 사용할 때 선택할 수 있는 시작 위치 id 목록 (start_locations.json 참조).

## `start_name`

(문자열)

시작 위치에 대해 표시되는 이름. 이것은 시나리오가 여러 시작 위치를 허용하지만 게임이 시나리오 설명에서 모두 나열할 수 없는 경우 유용합니다.
예를 들어: 시나리오가 야생 어딘가에서 시작할 수 있게 하면, 시작 위치에는 숲과 들판이 포함되지만, "start_name"은 단순히 "야생"일 수 있습니다.

## `professions`

(선택사항, 문자열 배열)

이 시나리오를 사용할 때 선택할 수 있는 허용된 직업 목록. 첫 번째 항목이 기본 직업입니다.
이것이 비어 있으면 모든 직업이 허용됩니다.

## `map_special`

(선택사항, 문자열)

시작 위치에 맵 스페셜을 추가합니다. 가능한 스페셜은 json_flags를 참조하세요.

## `missions`

(선택사항, 문자열 배열)

게임 시작 시 시작되고 플레이어에게 할당될 미션 id 목록.
ORIGIN_GAME_START 출처를 가진 미션만 허용됩니다. 목록의 마지막 미션이 활성 미션이 됩니다
(여러 미션이 할당된 경우).

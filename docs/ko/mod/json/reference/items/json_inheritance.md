# JSON 상속

JSON 데이터의 중복을 줄이기 위해 일부 타입은 기존 타입에서 상속받을 수 있습니다.

## 속성

### `abstract`

추상 아이템을 생성합니다 (게임에 포함되지 않고 JSON에서 복사되기 위해서만 존재하는 아이템). `"id"` _대신에_ 사용하세요.

### `copy-from`

속성을 복사하려는 아이템의 식별자입니다. 이를 통해 **동일한 타입의** 아이템의 정확한 복사본을 만들고 복사한 아이템에서 변경해야 하는 항목만 제공할 수 있습니다.

새 아이템에 `copy-from` 대상과 동일한 `"id"`를 부여하면 원본 아이템이 _대체됩니다_.

### `extends`

모더는 정의에 `"extends"` 필드를 추가하여 전체 목록을 덮어쓰는 대신 목록에 항목을 추가할 수 있습니다.

### `delete`

모더는 전체 목록을 덮어쓰는 대신 목록에서 요소를 제거하는 `"delete"` 필드를 추가할 수도 있습니다.

## 예시

다음 압축된 예시에서 `556` 탄약은 `copy-from`을 통해 `223` 탄약에서 파생됩니다:

```json
"id": "556",
"copy-from": "223",
"type": "AMMO",
"name": "5.56 NATO M855A1",
"description": "5.56x45mm ammunition with a 62gr FMJ bullet...",
"price": 3500,
"relative": {
    "damage": -2,
    "pierce": 4,
},
"extend": { "effects": [ "NEVER_MISFIRES" ] }
```

위 예시에는 다음 규칙이 적용됩니다:

- 누락된 필드는 부모와 동일한 값을 가집니다.

- 명시적으로 지정된 필드는 부모 타입의 필드를 대체합니다. 위 예시는 `name`, `description` 및 `price`를 대체합니다.

- 숫자 값은 부모에 `relative`로 지정할 수 있습니다. 예를 들어 `556`은 `223`보다 `damage`가 적지만 `pierce`가 더 많으며, `223`의 정의가 변경되면 이 관계를 유지합니다.

- 플래그는 `extend`를 통해 추가할 수 있습니다. 예를 들어 `556`은 군용 탄약이며 `NEVER_MISFIRES` 탄약 효과를 얻습니다. `223`에서 지정된 기존 플래그는 보존됩니다.

- 복사한 항목은 추가하거나 변경한 아이템과 동일한 `type`이어야 합니다 (모든 타입이 지원되는 것은 아니며, 아래 'support' 참조)

재장전된 탄약은 공장 동등품에서 파생되지만 `damage`와 `dispersion`에 10% 패널티가 있고 불발 가능성이 있습니다:

```json
"id": "reloaded_556",
"copy-from": "556",
"type": "AMMO",
"name": "reloaded 5.56 NATO",
"proportional": {
    "damage": 0.9,
    "dispersion": 1.1
},
"extend": { "effects": [ "RECYCLED" ] },
"delete": { "effects": [ "NEVER_MISFIRES" ] }
```

위 예시에는 다음 추가 규칙이 적용됩니다:

연쇄 상속이 가능합니다. 예를 들어 `reloaded_556`은 `556`에서 상속받으며, 이 자체는 `223`에서 파생됩니다.

숫자 값은 부모에 `proportional`로 십진 인수를 통해 지정할 수 있으며, 여기서 `0.5`는 50%이고 `2.0`은 200%입니다.

플래그는 `delete`를 통해 삭제할 수 있습니다. 삭제된 플래그가 부모에 존재하지 않아도 오류가 아닙니다.

다음 압축된 예시에서 `abstract` 타입을 정의하여 다른 타입이 상속받을 수만 있고 게임 자체에서는 사용할 수 없습니다. 다음 예시에서 `magazine_belt`는 구현된 모든 탄약 벨트에 공통적인 값을 제공합니다:

```json
"abstract": "magazine_belt",
"type": "MAGAZINE",
"name": "Ammo belt",
"description": "An ammo belt consisting of metal linkages which disintegrate upon firing.",
"rigid": false,
"armor_data": {
    "covers": [ "TORSO" ],
    ...
},
"flags": [ "MAG_BELT", "MAG_DESTROY" ]
```

위 예시에는 다음 추가 규칙이 적용됩니다:

필수 필드가 누락되어도 JSON 로딩이 완료된 후 `abstract` 타입이 폐기되므로 오류가 발생하지 않습니다.

선택적 필드가 누락되면 해당 타입의 일반적인 기본값으로 설정됩니다.

## 지원

다음 타입은 현재 상속을 지원합니다:

- GENERIC
- AMMO
- GUN
- GUNMOD
- MAGAZINE
- TOOL (하지만 TOOL_ARMOR는 지원 안 함)
- COMESTIBLE
- BOOK
- ENGINE

타입이 copy-from을 지원하는지 확인하려면 generic_factory를 구현했는지 알아야 합니다. 이를 확인하려면 다음을 수행하세요:

- [init.cpp](https://github.com/cataclysmbn/Cataclysm-BN/tree/main/src/init.cpp) 열기
- 타입을 언급하는 줄을 찾으세요. 예를 들어 `add( "gate", &gates::load );`
- 로드 함수를 복사하세요. 이 경우 _gates::load_가 됩니다.
- 이를 [github의 검색 바](https://github.com/cataclysmbn/Cataclysm-BN/search?q=%22gates%3A%3Aload%22&unscoped_q=%22gates%3A%3Aload%22&type=Code)에서 사용하여 _gates::load_를 포함하는 파일을 찾으세요.
- 검색 결과에서 [gates.cpp](https://github.com/cataclysmbn/Cataclysm-BN/tree/main/src/gates.cpp)를 찾습니다. 열어보세요.
- gates.cpp에서 generic_factory 줄을 찾으세요. 다음과 같이 보입니다:
  `generic_factory<gate_data> gates_data( "gate type", "handle", "other_handles" );`
- generic_factory 줄이 있으므로 copy-from을 지원한다고 결론을 내릴 수 있습니다.
- generic_factoy가 없으면 copy-from을 지원하지 않습니다. 예를 들어 vitamin 타입의 경우 (위 단계를 반복하면 [vitamin.cpp](https://github.com/cataclysmbn/Cataclysm-BN/tree/main/src/vitamin.cpp)에 generic_factoy가 포함되어 있지 않음을 알 수 있습니다)

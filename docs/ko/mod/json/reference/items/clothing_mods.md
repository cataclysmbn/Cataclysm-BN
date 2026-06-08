# 의류 모드

의류 모드는 방어구 및 의류 아이템에 적용하여 통계와 기능을 수정할 수 있는 업그레이드입니다. 이를 통해 플레이어는 방어구를 맞춤화하고 개선할 수 있습니다.

## 기본 구조

의류 모드는 `clothing_mod` 타입을 사용하여 정의됩니다:

```json
{
  "type": "clothing_mod",
  "id": "leather_padded",
  "name": "가죽 패딩",
  "description": "충격을 흡수하기 위해 의류에 추가 가죽 패딩을 추가합니다.",
  "encumbrance": 1,
  "warmth": 5,
  "bash_resist": 2,
  "cut_resist": 1
}
```

## 필드

| 필드          | 타입    | 설명                                                          |
| ------------- | ------- | ------------------------------------------------------------- |
| `type`        | string  | 반드시 `"clothing_mod"`여야 합니다.                           |
| `id`          | string  | 이 의류 모드의 고유 식별자.                                   |
| `name`        | string  | 플레이어에게 표시되는 이름.                                   |
| `description` | string  | 모드가 하는 일에 대한 설명.                                   |
| `encumbrance` | integer | 선택 사항. 이 모드로 인해 증가하는 부담. 양수 또는 음수 가능. |
| `warmth`      | integer | 선택 사항. 추가되는 따뜻함. 양수 또는 음수 가능.              |
| `bash_resist` | integer | 선택 사항. 타격 저항 증가.                                    |
| `cut_resist`  | integer | 선택 사항. 절단 저항 증가.                                    |
| `storage`     | string  | 선택 사항. 보관 용량 수정 (예: "1 L").                        |
| `coverage`    | integer | 선택 사항. 덮는 범위 수정.                                    |

## 예시

### 예시 1: 강화 패딩

```json
{
  "type": "clothing_mod",
  "id": "reinforced_padding",
  "name": "강화 패딩",
  "description": "타격 보호를 위해 두꺼운 패딩을 추가합니다.",
  "encumbrance": 2,
  "warmth": 3,
  "bash_resist": 4
}
```

### 예시 2: 경량 안감

```json
{
  "type": "clothing_mod",
  "id": "lightweight_lining",
  "name": "경량 안감",
  "description": "부담을 줄이기 위해 경량 안감으로 교체합니다.",
  "encumbrance": -1,
  "warmth": -2
}
```

## 참고사항

- 의류 모드는 일반적으로 제작 또는 특별한 수단을 통해 적용됩니다.
- 일부 모드는 양립할 수 없을 수 있습니다 (예: 동일한 아이템에 두 개의 다른 패딩 모드).
- 모드 효과는 기본 아이템 통계에 추가됩니다.

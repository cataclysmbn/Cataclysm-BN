# 폭발 데이터

이는 다양한 파일에서 재사용되는 인라인 데이터 조각입니다.
전역 참조를 위해 여기에 보관됩니다.

> [!CAUTION]
> 이 데이터는 독립형이 아니며 항상 다른 개체 내에 있습니다.

<a id="fields"></a>

### 필드

> [!NOTE]
> 여기에는 레거시 필드와 해당 필드의 기능이 나열되지 않습니다.

| 식별자          | 설명                                                                           |
| --------------- | ------------------------------------------------------------------------------ |
| damage          | (_선택적_) 폭발로 인한 피해량                                                  |
| radius          | (_선택사항_) 폭발 반경                                                         |
| fragment        | (_선택적_) [투사체](/mod/json/reference/misc/projectile/#fields)를 참조하세요. |
| fragment_effect | (_선택 사항_) [조각 효과](#fragment-effect) 배열                               |
| fire            | (_선택적_) 화재 필드도 생성합니다.                                             |

<a id="fragment-effect"></a>

#### 조각 효과

| 식별자    | 설명                                          |
| --------- | --------------------------------------------- |
| effect    | (_선택 사항_) 적용할 효과 ID                  |
| odds      | (_선택 사항_) 효과를 적용할 확률은 1배입니다. |
| min_turns | (_선택 사항_) 최소 회전 적용                  |
| max_turns | (_선택 사항_) 최대 회전 적용                  |

### 예

```json
{
  "damage": 50,
  "radius": 2,
  "fire": false,
  "fragment_effect": [{ "effect": "onfire", "odds": 2, "min_turns": 4, "max_turns": 8 }],
  "fragment": {
    "impact": { "damage_type": "bullet", "amount": 70, "armor_multiplier": 1 },
    "range": 5
  }
}
```

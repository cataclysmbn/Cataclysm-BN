# 발사체

이는 다양한 파일에서 재사용되는 인라인 데이터 조각입니다.
전역 참조를 위해 여기에 보관됩니다.

> [!CAUTION]
> 이 데이터는 독립형이 아니며 항상 다른 개체 내에 있습니다.

<a id="fields"></a>

### 필드

| 식별자            | 설명                                                                                        |
| ----------------- | ------------------------------------------------------------------------------------------- |
| impact            | (_필수_) 각 발사체에 대한 [피해 인스턴스](/mod/json/reference/misc/damage/#damage-instance) |
| range             | (_필수_) 발사체의 범위                                                                      |
| speed             | (_선택 사항_) 초당 미터 단위의 발사체 속도                                                  |
| aimedcritbonus    | (_선택 사항_) 치명타로 인한 최소 보너스                                                     |
| aimedcritmaxbonus | (_선택 사항_) 치명타로 인한 최대 보너스                                                     |

### 예

```json
{
  "impact": { "damage_type": "bullet", "amount": 70, "armor_multiplier": 1 },
  "range": 5,
  "speed": 1000,
  "aimedcritbonus": 0.0,
  "aimedcritmaxbonus": 0.0
}
```

# 피해

일반적으로 재사용되는 손상 인스턴스 및 유사한 인라인 값.

전역 참조를 위해 여기에 보관됩니다.

> [!CAUTION]
> 이 데이터는 독립형이 아니며 항상 다른 개체 내에 있습니다.

<a id="damage-instance"></a>

## 피해 인스턴스

손상 인스턴스를 정의하는 방법에는 두 가지가 있습니다. 하나는 [손상 단위](#damage-unit)인 것처럼 사용하는 것입니다.
다른 하나는 아래에 있어요

### 필드

| 식별자 | 설명                                    |
| ------ | --------------------------------------- |
| values | (_필수_) [손상 단위](#damage-unit) 배열 |

### 예

```json
{
  "values": [
    {
      "damage_type": "acid",
      "amount": 10,
      "armor_penetration": 4,
      "armor_multiplier": 2.5
    }
  ]
}
```

<a id="damage-unit"></a>

## 피해 유닛

### 필드

| 식별자            | 설명                                                           |
| ----------------- | -------------------------------------------------------------- |
| damage_type       | (_필수_) [손상 유형](#damage-types)                            |
| amount            | (_필수_) 얼마나 많은 피해를 입힐 것인가                        |
| armor_penetration | (_필수_) 갑옷 조각당 관통할 갑옷의 양                          |
| armor_multiplier  | (_필수_) armor_penetration이 적용된 후 남은 방어구에 대한 승수 |

### 예

```json
{
  "damage_type": "acid",
  "amount": 10,
  "armor_penetration": 4,
  "armor_multiplier": 2.5
}
```

<a id="damage-types"></a>

## 피해 유형

아래는 게임 내 모든 피해 유형 목록입니다.

- true
- biological
- bash
- cut
- acid
- stab
- bullet
- heat
- cold
- dark
- light
- psi
- electric

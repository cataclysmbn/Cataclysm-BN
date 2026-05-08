---
title: 재료 정의
---

> [!NOTE]
>
> 이 문서는 최근 `JSON INFO`에서 분리되었으며 추가 작업이 필요할 수 있습니다.

### 재료

| 식별자                 | 설명                                                                                                                                                            |
| ---------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `id`                   | 고유 ID. 소문자 snake_case. 연속된 하나의 단어여야 하며, 필요한 경우 밑줄을 사용하세요.                                                                         |
| `name`                 | 게임 내 표시되는 이름.                                                                                                                                          |
| `bash_resist`          | 재료가 타격 피해에 저항하는 정도.                                                                                                                               |
| `cut_resist`           | 재료가 절단 피해에 저항하는 정도.                                                                                                                               |
| `bullet_resist`        | 재료가 총알 피해에 저항하는 정도.                                                                                                                               |
| `acid_resist`          | 재료의 산 저항 능력.                                                                                                                                            |
| `elec_resist`          | 재료의 전기 저항 능력.                                                                                                                                          |
| `fire_resist`          | 재료의 화재 저항 능력.                                                                                                                                          |
| `chip_resist`          | 아이템 자체에 대한 공격으로 손상되는 것에 대한 저항을 반환합니다.                                                                                               |
| `bash_dmg_verb`        | 재료가 타격 피해를 받을 때 사용되는 동사.                                                                                                                       |
| `cut_dmg_verb`         | 재료가 절단 피해를 받을 때 사용되는 동사.                                                                                                                       |
| `dmg_adj`              | 손상된 아이템에 추가되는 설명으로 심각도가 오름차순으로 표시됩니다.                                                                                             |
| `dmg_adj`              | 재료의 손상 상태를 설명하는 데 사용되는 형용사.                                                                                                                 |
| `density`              | 재료의 밀도.                                                                                                                                                    |
| `vitamins`             | 재료의 비타민. 일반적으로 아이템별 값으로 덮어씁니다.                                                                                                           |
| `wind_resist`          | 백분율 0-100. 이 재료가 바람이 통과하는 것을 막는 효과. 높은 값이 더 좋습니다. 아이템을 구성하는 재료 중 어느 것도 값을 지정하지 않으면 기본값 99가 가정됩니다. |
| `warmth_when_wet`      | 완전히 젖었을 때 유지되는 따뜻함의 백분율. 기본값은 0.2입니다.                                                                                                  |
| `specific_heat_liquid` | 얼지 않았을 때 재료의 비열 (J/(g K)). 기본값 4.186.                                                                                                             |
| `specific_heat_solid`  | 얼었을 때 재료의 비열 (J/(g K)). 기본값 2.108.                                                                                                                  |
| `latent_heat`          | 재료의 융해 잠열 (J/g). 기본값 334.                                                                                                                             |
| `freeze_point`         | 이 재료의 어는점 (F). 기본값 32 F ( 0 C ).                                                                                                                      |
| `edible`               | 선택적 불리언. 기본값은 false입니다.                                                                                                                            |
| `rotting`              | 선택적 불리언. 기본값은 false입니다.                                                                                                                            |
| `soft`                 | 선택적 불리언. 기본값은 false입니다.                                                                                                                            |
| `reinforces`           | 선택적 불리언. 기본값은 false입니다.                                                                                                                            |

6개의 -resist 매개변수가 있습니다: acid, bash, chip, cut, elec, fire. 이들은 정수 값이며 기본값은 0이고 음수가 될 수 있어 더 많은 피해를 받을 수 있습니다.

```json
{
  "type": "material",
  "id": "hflesh",
  "name": "Human Flesh",
  "density": 5,
  "specific_heat_liquid": 3.7,
  "specific_heat_solid": 2.15,
  "latent_heat": 260,
  "edible": true,
  "rotting": true,
  "bash_resist": 1,
  "cut_resist": 1,
  "bullet_resist": 1,
  "acid_resist": 1,
  "fire_resist": 1,
  "elec_resist": 1,
  "chip_resist": 2,
  "dmg_adj": ["bruised", "mutilated", "badly mutilated", "thoroughly mutilated"],
  "bash_dmg_verb": "bruised",
  "cut_dmg_verb": "sliced",
  "vitamins": [["calcium", 0.1], ["vitB", 1], ["iron", 1.3]],
  "burn_data": [
    { "fuel": 1, "smoke": 1, "burn": 1, "volume_per_turn": "2500_ml" },
    { "fuel": 2, "smoke": 3, "burn": 2, "volume_per_turn": "10000_ml" },
    { "fuel": 3, "smoke": 10, "burn": 3 }
  ]
}
```

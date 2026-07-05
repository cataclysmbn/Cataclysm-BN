# 탄약 효과

탄약 효과에는 두 가지 유형이 있습니다. 하드코딩된 탄약 효과와 JSON화된 탄약 효과.

### 하드코딩된 탄약 효과

| 식별자                 | 설명                                                                              |
| ---------------------- | --------------------------------------------------------------------------------- |
| THROWN                 | 던져졌는지 아닌지 확인하는 데 사용됩니다.                                         |
| NO_PENETRATE_OBSTACLES | 장애물이 될 수 없습니다. 관통 울타리나 강화유리 등                                |
| HEAVY_HIT              | 트랩을 작동시키고 벽에 부딪힐 때 더 큰 소리를 냅니다.                             |
| NO_ITEM_DAMAGE         | 발사체에 맞은 아이템과 함정은 손상되지 않습니다.                                  |
| NOGIB                  | 탄약은 시체를 과도하게 죽이거나 폭발시키지 않습니다.                              |
| COOKOFF                | 탄약은 화재로 폭발합니다                                                          |
| INCENDIARY             | 지형/표적에 화재 발생                                                             |
| NEVER_MISFIRES         | 불발되지 않음                                                                     |
| ACT_ON_RANGED_HIT      | 아이템이 적중 시 사용 동작을 실행합니다.                                          |
| APPLY_SAP              | 피해 규모를 줄이는 턴에 교란 효과를 적용합니다.                                   |
| BEANBAG                | 짧은 시간 동안 기절 효과를 적용합니다.                                            |
| BLINDS_EYES            | 눈에 닿으면 실명 적용                                                             |
| BOUNCE                 | 근처 지역의 다른 대상을 튕겨서 맞추면 `WIDE` 효과도 제공됩니다.                   |
| BURST                  | 충격을 받으면 내용물을 떨어뜨리지만 파괴됩니다.                                   |
| CUSTOM_EXPLOSION       | 아이템의 폭발 데이터를 사용합니다.                                                |
| DRAW_AS_LINE           | 애니메이션 없이 즉시 그려집니다.                                                  |
| IGNITE                 | 적중한 모든 생물을 불태웁니다.                                                    |
| JET                    | 피해가 0이 되어도 효과 전체를 끌어냅니다.                                         |
| LARGE_BEANBAG          | BEANBAG 기절 효과를 더 오랫동안 적용합니다.                                       |
| MUZZLE_SMOKE           | 발사 시 작은 연기 기둥을 생성합니다.                                              |
| NO_CRIT                | 치명타를 칠 수는 없지만 여전히 방목할 수는 있습니다.                              |
| NO_DAMAGE              | 어떤 피해도 적용되지 않습니다.                                                    |
| NO_DAMAGE_SCALING      | 치명타 불가, 방목 불가                                                            |
| NO_EMBED               | 생물에 삽입할 수 없습니다.                                                        |
| NO_OVERSHOOT           | 누락된 경우에도 궤적은 패스 대상을 확장할 수 없습니다.                            |
| NULL_SOURCE            | 내부적으로 사용되며 원산지는 실제 동물이 아닙니다.                                |
| POISON                 | 적중 시 독을 적용하고, 치명타 시 더 강한 독을 적용합니다.                         |
| PARALYZEPOISON         | 적중 시 마비 독 적용                                                              |
| SHATTER_SELF           | 적중 시 던져지고 파괴되고, 내용물이 떨어지고 소음이 발생합니다.                   |
| TANGLE                 | 생물에 묶인 효과를 적용하여 복구할 수 있습니다.                                   |
| NET_TANGLE             | 그룹에 묶인 효과를 적용하며 복구할 수 없습니다.                                   |
| WHIP                   | 야생동물에게 공격을 가할 때 겁을 주고, 도달 공격 시 때때로 무장해제를 적용합니다. |
| BLACKPOWDER            | 손상을 주는 덩어리가 쌓이게 됩니다.                                               |
| RECYCLED               | 256분의 1의 확률로 발사가 실패합니다.                                             |
| RECOVER_X              | 충격 시 사용한 탄약을 1회 충전할 수 있는 (X-1/X) 확률이 있습니다.                 |
| WIDE                   | 현재는 효과가 없습니다. `HARDTOSHOOT`이 효과가 없도록 해야 합니다.                |
| SHOT                   | 강제로 `WIDE` 효과를 적용합니다.                                                  |

### 필드

| 식별자       | 설명                                                                                         |
| ------------ | -------------------------------------------------------------------------------------------- |
| id           | (_필수_) 고유 ID입니다.                                                                      |
| aoe          | (_선택적_) 객체, 필드는 [aoe](#aoe) 참조                                                     |
| trail        | (_선택적_) 객체, 필드는 [트레일](#trail) 참조                                                |
| explosion    | (_선택 사항_) 객체, 필드는 [폭발](/mod/json/reference/misc/explosions/#fields)을 참조하세요. |
| do_flashbang | (_선택적_) 섬광탄 효과를 냅니다.                                                             |
| do_emp_blast | (_선택 사항_) Emp 효과 수행                                                                  |

<a id="aoe"></a>

#### 아오에

대상을 중심으로 한 효과

| 식별자            | 설명                                                              |
| ----------------- | ----------------------------------------------------------------- |
| field_type        | (_선택적_) 효과에 사용할 필드의 ID                                |
| intensity_min     | (_선택 사항_) 필드의 최소 강도                                    |
| intensity_max     | (_선택 사항_) 필드의 최대 강도                                    |
| radius            | (_선택 사항_) 효과 반경                                           |
| radius_z          | (_선택 사항_) Z축 효과 반경                                       |
| chance            | (_선택 사항_) 추가된 필드의 확률(%)                               |
| size              | (_선택 사항_) Aoe의 대략적인 크기                                 |
| check_sees        | (_선택적_) 소스는 효과의 위치를 확인해야 하며, 조건 아래의 효과는 |
| check_sees_radius | (_선택 사항_) 소스는 반경 내의 모든 지점을 확인해야 합니다.       |
| check_passable    | (_선택 사항_) 대상은 통과 가능한 타일에 있어야 합니다.            |

<a id="trail"></a>

#### 트레일

모든 효과는 발사체의 경로를 따르는 트레일에 적용됩니다.

| 식별자        | 설명                                |
| ------------- | ----------------------------------- |
| field_type    | (_선택적_) 효과에 사용할 필드의 ID  |
| intensity_min | (_선택 사항_) 필드의 최소 강도      |
| intensity_max | (_선택 사항_) 필드의 최대 강도      |
| chance        | (_선택 사항_) 추가된 필드의 확률(%) |

### 예

```json
{
  "id": "MININUKE_MOD",
  "type": "ammo_effect",
  "aoe": {
    "field_type": "fd_nuke_gas",
    "intensity_min": 3,
    "intensity_max": 3,
    "chance": 100,
    "radius": 24,
    "radius_z": 0,
    "size": 1,
    "check_passable": true,
    "check_sees": true,
    "check_sees_radius": 3
  },
  "trail": { "field_type": "fd_acid", "intensity_min": 1, "intensity_max": 3, "chance": 66 },
  "explosion": {
    "damage": 3000,
    "radius": 20,
    "fragment": {
      "impact": { "damage_type": "heat", "amount": 3000, "armor_multiplier": 2 },
      "range": 24
    }
  }
}
```

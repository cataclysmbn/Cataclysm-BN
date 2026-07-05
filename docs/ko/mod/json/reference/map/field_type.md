# 필드 타입

```jsonc
{
  // 필수 데이터
  "type": "field_type", // 이것은 필드 타입입니다
  "id": "fd_gum_web", // 필드의 이드
  // 필드의 강도 배열.
  // 첫 번째는 강도 1이며 매번 1씩 증가합니다.
  "intensity_levels": [
    {
      "name": "shadow", // 이 강도 수준의 이름
      "sym": "a", // 이 전계 강도 수준에 대한 기호
      "color": "light_red", // 필드 기호의 저주 색상
      // 가시성
      "transparent": false, // 날씨 필드를 통해 볼 수 있습니다
      "light_override": 3.7, // 사용되는 빛의 양은 주변광을 무시합니다. 100은 일광이고 0은 100% 어두움입니다.
      "light_color": "#ffd08a", // 방출하는 빛의 색
      "light_emitted": 20, // 방출하는 빛의 양
      // 운동
      "dangerous": true, // 이 정도 강도는 견디기 위험한 날씨
      "move_cost": 50, // 현장 이동 비용
      // 방사선
      "extra_radiation_min": 10, // 현장 진입으로 인한 추가 방사선의 최소량
      "extra_radiation_max": 100, // 현장 진입으로 인한 추가 방사선의 최대량
      "radiation_hurt_damage_min": 1, // 모든 신체 부위에 유발되는 최소한의 통증
      "radiation_hurt_damage_max": 3, // 신체 모든 부위에 야기할 수 있는 최대 통증량
      "radiation_hurt_message": "The radioactive gas burns!", // 통증이 있을 때 인쇄할 메시지
      // 강도 조작
      "intensity_upgrade_chance": 10, // 몇 틱마다 강도를 업그레이드할 수 있는 x의 기회
      "intensity_upgrade_duration": 1, // 강도를 업그레이드하기 전에 기다려야 하는 틱 수
      // 몬스터 그룹 생성
      "monster_spawn_chance": 10, // 몬스터가 나올 확률
      "monster_spawn_count": 10, // 스폰할 몬스터의 수
      "monster_spawn_radius": 10, // 몬스터가 스폰되는 반경
      "monster_spawn_group": "GROUP_ZOMBIE", // 생성할 그룹
      // 기타
      "convection_temperature_mod": 10, // 추가할 온도의 양
      // 효과
      "effects": [{
        // 기본정보
        "effect_id": "sap", // 적용할 효과 ID
        "body_part": "torso", // 명시된 ID가 있는 본문 부분
        "intensity": 1, // 효과의 강도
        // 기간
        "max_duration": "2 seconds", // 효과의 최대 지속 시간
        "min_duration": "2 seconds", // 효과의 최소 지속 시간
        // 면역력 및 적용 가능성
        "is_environmental": true, // 환경 보호가 보호할 것인가
        "immune_in_vehicle": true, // 차량 부품에 닿으면 면역됩니다.
        "chance_in_vehicle": 10, // 위 조건 충족 시 x 확률로 효과 획득
        "immune_inside_vehicle": true, // 내부 자격을 갖춘 차량 부품에 있을 때 면역
        "chance_inside_vehicle": 10, // 위 조건 충족 시 x 확률로 효과 획득
        "immune_outside_vehicle": true, // 내부로 인정되는 차량 부품이 아닐 때 면역
        "chance_outside_vehicle": 10, // 위 조건 충족 시 x 확률로 효과를 얻습니다.
        // 메시지
        "message": "The sap is sticky!", // 효과 수신 시 인쇄할 메시지
        "message_npc": "The sap sticks to <npcname>!", // NPC이 적용되면 인쇄할 메시지
        "message_type": "bad", // 이것은 어떤 유형의 메시지입니까?
      }],
    },
  ],
  // 그래픽
  "display_items": true, // 파일의 항목을 표시하시겠습니까?
  "display_field": true, // 필드를 표시할지 말지?
  "priority": 1, // 필드의 심볼/스프라이트 표시 우선순위
  // 면역
  "immunity_data": {
    "traits": ["M_IMMUNE"], // 현장의 효과를 방지하는 특성 배열
    "body_part_env_resistance": [["mouth", 15], ["eyes", 15]], // 현장으로부터 보호하기 위해 신체 부위의 환경 저항이 필요함
  },
  "immune_mtypes": ["mon_zombie"], // 필드 면역인 몬스터 타입
  "has_fire": true, // 화재 면역으로 인해 이 필드에 면역이 됩니까?
  "has_acid": true, // 산성 면역이 있습니까?
  "has_elec": true, // 전기는 어떻습니까?
  "has_fume": true, // 저주에 배경색을 흰색으로 바꿔볼까요?
  // 부패와 확산
  "underwater_age_speedup": "1 minute", // 더 많은 회전이 물 속에서의 나이를 전달합니다.
  "outdoor_age_speedup": "1 minute", // 물 속에 있는 동안 더 많은 회전이 나이를 전달합니다.
  "decay_amount_factor": 5, // 필드의 나이에 대한 또 다른 턴 증가
  "percent_spread": 10, // 매 턴마다 퍼질 확률(%)
  "half_life": "10 minutes", // 절반이 사라질 때까지의 회전 수
  "stacking_type": "duration", // 더 많은 필드가 `intensity` 또는 `duration`를 추가합니까?
  "wandering_field": "fd_toxic_gas", // 이 필드가 생성하는 필드
  "accelerated_decay": true, // 이 필드는 지형 이동에 따라 아래쪽으로 이동합니까?
  // 기타
  "gas_absorbption_factor": 10, // 방독면에서 먹을 충전 횟수
  "dirty_transparency_cache": true, // 투명도 캐시를 강제로 다시 계산하는 날씨입니다. transparent가 false로 자동 설정됩니다.
  "description_affix": "covered in", // 모든 강도 레벨 이름 앞에 이것을 넣습니다.
  "phase": "solid", // 고체 액체 가스인가요 아니면 플라즈마인가요? 주로 가스가 전파에 영향을 미칩니다.
  "is_splattering": true, // 혈액을 자동차 부품에 적용합니까?
  "apply_slime_factor": 2, // 인자 * 강도의 향기를 적용합니다.
  "scent_neutralization": 2, // 이 값만큼 향기를 줄입니다.
  // NPC 불평하다
  "npc_complain": {
    "chance": 20, // 불평할 기회는 x 중 하나
    "issue": "crack_smoke", // 불만을 토로하는 Talk 태그
    "duration": "30 minutes", // 언제까지 불평만 할 것인가
    "speech": "<crack_smoke>", // 다시 한 번 말했던 talk 태그
  },
  // 몬스터 산란
  "monster_spawn_chance": 2, // x번 중 하나의 기회로 몬스터를 생성할 수 있습니다.
  "monster_spawn_count": 3, // 얼마나 많은 몬스터가 스폰될까요?
  "monster_spawn_group": "GROUP_ZOMBIE", // 어떤 몬스터 그룹을 생성할지
  "monster_spawn_radius": 5, // 몬스터 생성 위치의 타일 반경
  // 배시 데이터
  "bash": {
    "str_min": 1, // 강타에 필요한 강타 피해의 하부 브래킷
    "str_max": 3, // 상위 브래킷
    "sound_vol": 2, // 필드를 성공적으로 강타할 때 발생하는 소음
    "sound_fail_vol": 2, // 필드를 강타하지 못했을 때 발생하는 소음
    "sound": "shwip", // 성공 시 소리
    "sound_fail": "shwomp", // 실패 시 소리
    "msg_success": "You brush the gum web aside.", // 성공 메시지
    "move_cost": 120, // 해당 필드를 성공적으로 강타하는 데 드는 이동 횟수(기본값: 100)
    "items": [ // 강타 성공 시 드랍되는 아이템
      { "item": "2x4", "count": [5, 8] },
      { "item": "nail", "charges": [6, 8] },
      {
        "item": "splinter",
        "count": [3, 6],
      },
      { "item": "rag", "count": [40, 55] },
      { "item": "scrap", "count": [10, 20] },
    ],
  },
}
```

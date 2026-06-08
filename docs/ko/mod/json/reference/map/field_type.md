# 필드 타입

```json
{
  "type": "field_type", // 이것은 필드 타입입니다
  "id": "fd_gum_web", // 필드의 id
  "immune_mtypes": ["mon_spider_gum"], // 이 필드에 면역인 몬스터 목록
  "intensity_levels": [
    {
      "name": "shadow", // 이 강도 레벨의 이름
      "light_override": 3.7
    }
    //이 필드가 차지하는 타일의 조명 레벨은 주변 조명에 관계없이 3.7로 설정됩니다.
  ],
  "bash": {
    "str_min": 1, // 부수기에 필요한 부수기 데미지의 하한
    "str_max": 3, // 상한
    "sound_vol": 2, // 필드를 성공적으로 부술 때 나는 소음
    "sound_fail_vol": 2, // 필드를 부수는 데 실패할 때 나는 소음
    "sound": "shwip", // 성공 시 소리
    "sound_fail": "shwomp", // 실패 시 소리
    "msg_success": "You brush the gum web aside.", // 성공 시 메시지
    "move_cost": 120, // 해당 필드를 성공적으로 부수는 데 드는 이동 비용 (기본값: 100)
    "items": [ // 성공적으로 부술 때 떨어지는 아이템
      { "item": "2x4", "count": [5, 8] },
      { "item": "nail", "charges": [6, 8] },
      {
        "item": "splinter",
        "count": [3, 6]
      },
      { "item": "rag", "count": [40, 55] },
      { "item": "scrap", "count": [10, 20] }
    ]
  }
}
```

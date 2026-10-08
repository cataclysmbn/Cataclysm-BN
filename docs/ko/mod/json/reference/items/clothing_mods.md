---
title: 의류 개조
---

### clothing_mod

```json
"type": "clothing_mod", // 의류 개조로 정의합니다.
"id": "leather_padded", // 고유 ID입니다.
"flag": "leather_padded", // 의류에 추가할 플래그입니다.
"item": "leather", // 소비할 아이템입니다.
"implement_prompt": "Pad with leather", // 개조를 적용할 때 표시할 메시지입니다.
"destroy_prompt": "Destroy leather padding", // 개조를 제거할 때 표시할 메시지입니다.
"restricted": true, // (선택 사항) true이면 의류의 "valid_mods" 목록에 이 개조의 플래그가 있어야 사용할 수 있습니다. 기본값은 false입니다.
"use_base_material": true, // (선택 사항) true이면 item 필드의 아이템 대신 의류의 기본 재료 중 하나를 사용합니다. 기본값은 false입니다.
"mod_value": [ // 개조 효과 목록입니다.
    {
        "type": "bash", // 사용 가능한 효과: "bash", "cut", "bullet", "fire", "acid", "warmth", "storage", "encumbrance".
        "value": 1, // 효과의 값입니다.
        "round_up": false, // (선택 사항) 효과 값을 올림할지 여부입니다. 기본값은 false입니다.
        "proportion": [ // (선택 사항) 의류 속성에 비례해 효과 값을 조정합니다.
            "thickness", // 아이템의 "material_thickness" 각 층마다 값을 더합니다.
            "volume", // 아이템의 기본 부피 각 리터마다 값을 더합니다.
            "coverage" // 아이템의 평균 덮임 비율에 따라 값을 줄입니다.
        ]
    }
]
```

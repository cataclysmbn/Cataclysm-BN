---
제목: 중첩된 레시피 카테고리
---

### 중첩된 레시피 카테고리

중첩된 카테고리를 사용하면 제작 UI 안에 계층적인 그룹을 만들 수 있습니다. 이는 `type: "nested_category"`를 가진 JSON 객체로, 레시피처럼 목록에 표시되고 필터링되지만 아이템을 생성하지는 않습니다. 중첩된 카테고리는 접을 수 있는 항목으로 표시되며 레시피(또는 다른 중첩 카테고리)로 확장됩니다. `nested_category_data`에 나열합니다.

새로운 제작 탭을 추가하지 않고 하위 카테고리 안에 추가적인 그룹화 계층을 만들고 싶거나 하위 카테고리에 항목이 너무 많을 때 중첩된 카테고리를 사용하세요.

### 필드

| 식별자                 | 설명                                                                                           |
| ---------------------- | ---------------------------------------------------------------------------------------------- |
| `id`                   | 중첩된 카테고리의 고유 ID입니다.                                                               |
| `type`                 | `nested_category`여야 합니다.                                                                  |
| `nested_name`          | 제작 UI에 표시되는 이름입니다. 중첩된 카테고리에는 `result`가 없으므로 사용하는 것이 좋습니다. |
| `category`             | 항목이 표시되는 제작 카테고리(`CC_*`)입니다.                                                   |
| `subcategory`          | 항목이 표시되는 제작 하위 카테고리입니다.                                                      |
| `description`          | 제작 UI에 표시되는 선택적 설명입니다.                                                          |
| `nested_category_data` | 이 항목 아래에 표시할 레시피 ID 또는 중첩 카테고리 ID의 배열입니다.                            |

### 동작 참고 사항

- `nested_category_data`는 레시피 자체의 ID로 표준 레시피를, `id`로 다른 중첩 카테고리를 참조할 수 있습니다. 레시피 ID는 `id_suffix`가 적용된 결과 아이템 ID가 아닙니다.
- 중첩된 카테고리에는 `result`, 구성 요소, 도구 또는 시간을 정의하지 마세요. UI 전용 항목이므로 제작할 수 없습니다.
- 중첩된 카테고리 사이의 순환 참조를 피하세요. 순환은 UI를 표시할 때 무시될 수 있으며 항목이 숨겨질 수 있습니다.

### 예시

```json
[
  {
    "type": "nested_category",
    "id": "hot_drinks",
    "nested_name": "hot drinks",
    "category": "CC_FOOD",
    "subcategory": "CSC_FOOD_DRINKS",
    "description": "Warm beverages and infusions.",
    "nested_category_data": [
      "coffee",
      "tea",
      "herbal_tea"
    ]
  }
]
```

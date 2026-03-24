### 도구 품질

> [!NOTE]
>
> 이 문서는 새로 작성되었으며 더 많은 예시와 링크가 필요할 수 있습니다.

도구 품질은 레시피 및 기타 시스템에서 사용되는 명명된 능력 등급을 정의합니다. `data/json/tool_qualities.json`에서 로드됩니다.

```json
{
  "type": "tool_quality", // 도구 품질 정의
  "id": "SEW", // 고유 품질 ID
  "name": { "str": "sewing" }, // 품질에 대한 표시 이름
  "crafting_speed_bonus_per_level": 1.1, // 선택 사항, 레벨당 배율 (아래 참조)
  "crafting_speed_level_offset": 2 // 선택 사항, 레벨당 오프셋 (아래 참조)
}
```

#### 필드

- `id`: 고유 품질 ID.
- `name`: 품질에 대한 표시 이름.
- `crafting_speed_bonus_per_level`: 선택 사항 (기본값 = 0.0). 레시피 요구사항을 초과하는 품질 레벨당 제작 속도 배율 (예: 추가 레벨당 +10%의 경우 `1.1`), 아이템이 자체 `crafting_speed_modifier`를 정의하지 않은 경우에만 적용됩니다.
- `crafting_speed_level_offset`: 선택 사항 (기본값 = 0). `crafting_speed_bonus_per_level`이 적용되기 시작하는 최소 품질 레벨, 레시피가 더 낮은 레벨을 요구하더라도 적용됩니다.

#### 제작 사용

도구 품질은 레시피에서 많은 동등한 아이템을 단일 능력으로 그룹화하는 데 사용됩니다. `qualities` 배열에 품질을 나열하는 모든 아이템은 요청된 레벨 이상에서 레시피 `qualities` 요구사항을 충족할 수 있습니다. 이를 통해 모든 허용 가능한 도구를 명시적으로 나열하는 것을 피할 수 있습니다.

예시 레시피 요구사항:

```json
"qualities": [ { "id": "SEW", "level": 2 } ]
```

예시 아이템:

```json
"qualities": [ [ "SEW", 2 ] ]   // 기본 재봉 도구
"qualities": [ [ "SEW", 4 ] ]   // 고품질 재봉 키트
```

레시피가 `SEW` 2를 요구하는 경우, `SEW` 2 이상의 모든 아이템을 사용할 수 있습니다. 품질이 `crafting_speed_bonus_per_level`을 정의하는 경우, 요구사항을 초과하는 더 높은 레벨은 아이템이 자체 `crafting_speed_modifier`를 제공하지 않는 한 제작 속도를 증가시킵니다. `crafting_speed_level_offset`이 설정된 경우, 속도 보너스는 레시피 레벨과 오프셋 중 더 높은 값에서 시작됩니다.

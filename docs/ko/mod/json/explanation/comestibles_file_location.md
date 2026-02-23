# 소비재 파일 위치

소비재(음식, 음료 등)는 다음 위치에 정의되어 있습니다:

## 기본 게임

- `data/json/items/comestibles/` - 모든 소비재 JSON 파일

## 모드

모드는 자체 소비재를 추가하거나 기존 소비재를 수정할 수 있습니다:

- `data/mods/[MOD_NAME]/items/comestibles/`

## 구성

소비재는 다음과 같이 정의됩니다:

```json
{
  "type": "COMESTIBLE",
  "id": "example_food",
  "name": "example food",
  "description": "An example food item.",
  ...
}
```

자세한 내용은 [COMESTIBLES.md](../reference/items/COMESTIBLES.md)를 참조하세요.

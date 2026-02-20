# 모드 타일셋

모드 타일셋을 사용하면 모드 작성자가 메인 게임 타일셋을 수정하지 않고 맞춤 모드 콘텐츠를 위한 타일을 제공할 수 있습니다. 이는 모드별 아이템, 몬스터, 지형 또는 기타 엔티티에 타일을 추가하는 데 유용합니다.

## 기본 설정

모드에 타일을 추가하려면 모드 디렉토리에 `mod_tileset` 디렉토리를 만드세요:

```
my_mod/
  modinfo.json
  mod_tileset/
    mod_tileset.json
    tiles.png
```

## mod_tileset.json

`mod_tileset.json` 파일은 타일 매핑을 정의합니다:

```json
[
  {
    "type": "mod_tileset",
    "compatibility": ["MSXotto", "UltimateCataclysm"],
    "tiles-new": [
      {
        "file": "tiles.png",
        "sprite_width": 32,
        "sprite_height": 32,
        "sprite_offset_x": 0,
        "sprite_offset_y": 0,
        "tiles": [
          {
            "id": "my_custom_item",
            "fg": 0,
            "bg": 0
          }
        ]
      }
    ]
  }
]
```

### 필드

| 필드            | 타입   | 설명                                        |
| --------------- | ------ | ------------------------------------------- |
| `type`          | string | 반드시 `"mod_tileset"`이어야 합니다.        |
| `compatibility` | array  | 이 모드 타일셋과 호환되는 타일셋 이름 목록. |
| `tiles-new`     | array  | 타일 이미지 파일과 매핑을 정의하는 배열.    |

`tiles-new`의 필드는 [외부 타일셋](external_tileset.md)과 동일합니다.

## 호환성

`compatibility` 필드는 모드 타일이 작동할 타일셋을 지정합니다. 일반적인 타일셋 이름:

- `"MSXotto"` - MSX++DEAD_PEOPLE 타일셋
- `"UltimateCataclysm"` - UltimateCataclysm 타일셋
- `"UNDEAD_PEOPLE"` - UndeadPeople 타일셋

여러 타일셋에 대한 호환성을 지정하거나 모든 타일셋에 대해 `["*"]`를 사용할 수 있습니다.

## 예시: 간단한 모드 타일

```json
[
  {
    "type": "mod_tileset",
    "compatibility": ["MSXotto"],
    "tiles-new": [
      {
        "file": "my_items.png",
        "sprite_width": 32,
        "sprite_height": 32,
        "tiles": [
          { "id": "my_magic_sword", "fg": 0 },
          { "id": "my_magic_shield", "fg": 1 }
        ]
      }
    ]
  }
]
```

이것은 32x32 타일셋을 사용하는 MSXotto 타일셋에 두 개의 맞춤 아이템 타일을 추가합니다.

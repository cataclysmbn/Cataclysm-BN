# 모드 타일셋

모드 타일셋은 추가 스프라이트 시트를 정의합니다. `type` 멤버가 `mod_tileset`인 JSON 객체로 지정합니다.

예:

```json
[
  {
    "type": "mod_tileset",
    "compatibility": ["MshockXottoplus"],
    "tiles-new": [
      {
        "file": "test_tile.png",
        "tiles": [
          {
            "id": "player_female",
            "fg": 1,
            "bg": 0
          },
          {
            "id": "player_male",
            "fg": 2,
            "bg": 0
          }
        ]
      }
    ]
  }
]
```

## `compatibility`

(문자열)

호환되는 타일셋의 내부 ID입니다. 기본 타일셋의 ID가 이 필드에 있을 때만 모드 타일셋을 적용합니다.

## `tiles-new`

스프라이트 시트 설정입니다. `tile_config`의 `tiles-new` 필드와 같습니다. 스프라이트 파일은 JSON 파일이 있는 폴더에서 불러옵니다.

## `state-modifiers`

상태 수정자를 사용하면 모드 타일셋이 캐릭터 상태에 따른 UV 기반 스프라이트 수정을 정의하거나 재정의할 수 있습니다. 모드 타일셋이 기본 타일셋과 같은 `id`를 가진 상태 수정자 그룹을 정의하면 모드의 정의가 기본 타일셋의 정의를 대체합니다.

```json
{
  "type": "mod_tileset",
  "compatibility": ["UndeadPeopleTileset"],
  "tiles-new": [
    {
      "file": "uv-tiles.png",
      "tiles": [],
      "state-modifiers": [
        {
          "id": "movement_mode",
          "override": false,
          "use_offset": false,
          "tiles": [
            { "id": "walk", "fg": null },
            { "id": "crouch", "fg": 1 },
            { "id": "run", "fg": 2 }
          ]
        }
      ]
    }
  ]
}
```

`state-modifiers` 배열은 `tiles-new` 항목에서 `tiles`와 나란히 배치합니다. 각 수정자 그룹에는 다음이 필요합니다.

| 필드         | 타입   | 설명                                                    |
| ------------ | ------ | ------------------------------------------------------- |
| `id`         | string | 그룹 식별자(`movement_mode`, `downed`, `lying_down`).   |
| `override`   | bool   | 이 상태가 활성화되면 우선순위가 낮은 그룹을 건너뜁니다. |
| `use_offset` | bool   | 오프셋 모드는 `true`, 정규화 UV 모드는 `false`입니다.   |
| `tiles`      | array  | 상태와 스프라이트의 매핑입니다.                         |

UV 매핑 모드와 수정자 스프라이트 제작에 대한 전체 설명은 [타일셋의 상태 수정자 절](./tileset.md#state-modifiers)을 참조하세요.

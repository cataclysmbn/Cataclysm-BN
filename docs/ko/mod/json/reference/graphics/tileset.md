# 타일셋

타일셋은 게임에 그래픽 이미지를 제공합니다. 각 타일셋에는 이미지 스프라이트로 이루어진 타일시트가 하나 이상 있고, 스프라이트 시트의 내용을 게임의 여러 엔티티에 매핑하는 방법을 설명하는 `tile_config.json` 파일이 있습니다. 메타데이터를 제공하는 `tileset.txt` 파일도 있습니다.

## 타일셋 합성

2019년 10월 이전에는 각 타일시트를 완전히 합성한 상태로 저장소에 제출하고 `tile_config.json`의 스프라이트 인덱스를 직접 계산해야 했습니다. 이제는 타일셋을 개별 스프라이트 파일과 스프라이트 파일명을 사용하는 `tile_entry` JSON 파일의 디렉터리로 제출할 수 있습니다. TypeScript 타일셋 도구는 이 파일들을 타일시트로 병합하고, 스프라이트 파일명을 스프라이트 인덱스로 변환한 뒤 `tile_config.json`을 작성합니다.

이 문서에서는 완전히 합성된 타일시트로 제출하는 타일셋을 레거시 타일셋, 개별 스프라이트 이미지 파일로 제출하는 타일셋을 합성형 타일셋이라 부릅니다.

<a id="typescript-tileset-tool"></a>

### TypeScript 타일셋 도구

일반적인 합성/분해 작업에는 `scripts/tileset.ts`를 사용합니다. 이 도구는 [PR #8151](https://github.com/cataclysmbn/Cataclysm-BN/pull/8151)에서 기존 Python 합성 작업 흐름을 대체했습니다.

```sh
# 개별 스프라이트 파일과 tile_entry를 tile_config.json 및 타일시트 PNG로 합성합니다.
deno run -A scripts/tileset.ts --pack gfx/Retrodays

# 합성 결과를 다른 디렉터리에 씁니다.
deno run -A scripts/tileset.ts --pack gfx/Retrodays /tmp/Retrodays-composed

# 레거시 타일셋을 개별 스프라이트 파일과 tile_entry로 분해합니다.
deno run -A scripts/tileset.ts --unpack gfx/ChestHole16Tileset
```

유용한 옵션:

- `--format-json`: 생성된 `tile_config.json`을 보기 좋게 정렬합니다.
- `--only-json`: 타일시트 PNG를 쓰지 않고 `tile_config.json`만 다시 생성합니다.
- `--use-all`: 원래 사용되지 않는 PNG 파일도 파일명 기반 ID로 포함합니다.
- `--palette`와 `--palette-copies`: 8비트 팔레트 출력을 생성합니다.

기존 `tools/gfx_tools/compose.py`와 `tools/gfx_tools/decompose.py` 스크립트는 문서 및 일상적인 타일셋 작업에 더 이상 권장하지 않습니다. 호환성과 동등성 테스트를 위해 저장소에는 남아 있습니다.

합성해도 원본 스프라이트 파일과 `tile_entry` JSON 파일은 보존됩니다.

작은 예제나 브라우저에서 빠르게 확인할 작업은 [타일셋 웹 도구](/dev/reference/tileset_web_tool/)를 참조하세요.

### 디렉터리 구조

각 합성형 타일셋에는 `pngs_tree_32x40`이나 `pngs_overlay`처럼 이름이 `pngs_`로 시작하는 디렉터리가 하나 이상 있습니다. 이를 이미지 디렉터리라 합니다. 이미지 디렉터리 안의 모든 스프라이트는 높이와 너비가 같아야 하며 하나의 타일시트로 병합됩니다.

이미지 디렉터리 이름에 스프라이트 크기를 넣기를 권장하지만 필수는 아닙니다. `pngs_overlay`보다 `pngs_overlay_24x24`를 권장하지만 둘 다 허용됩니다. 이미지 디렉터리마다 별도의 타일시트를 만들며, 성능상 타일시트는 가능한 한 커야 하므로 이미지 디렉터리 수를 최소화하기를 강력히 권장합니다.

각 이미지 디렉터리에는 하위 디렉터리 계층, `tile_entry` JSON 파일, 스프라이트 파일이 들어 있습니다. 확장 타일시트용 `tile_entry` JSON 파일은 이미지 디렉터리의 최상위에 있어야 한다는 점을 제외하면 파일의 배치나 이름에는 제한이 없습니다. 하위 디렉터리는 필수가 아니지만 관리를 쉽게 하려면 사용하는 것이 좋습니다.

#### `tile_entry` JSON

각 `tile_entry` JSON은 하나 이상의 게임 엔티티를 하나 이상의 스프라이트에 매핑하는 방법을 설명하는 딕셔너리입니다. 가장 단순한 형태에는 게임 엔티티 하나, 전경 스프라이트 하나, 선택적인 배경 스프라이트 하나, 회전 값이 있습니다. 예:

```cpp
{                                           // 이것은 객체이며 목록일 필요가 없습니다
    "id": "mon_cat",                        // 이 스프라이트가 나타내는 게임 엔티티
    "fg": "mon_cat_black",                  // 스프라이트 이름
    "bg": "shadow_bg_1",                    // 스프라이트 이름. 항상 단일 값입니다
    "rotates": false                        // 차량 부품처럼 회전하는 대상은 true
}
```

`"id"`, `"fg"`, `"bg"`의 값은 한 이미지 디렉터리 안이나 서로 다른 이미지 디렉터리에서 반복해서 사용할 수 있습니다. `"fg"`와 `"bg"` 스프라이트 이미지는 이미지 디렉터리를 가로질러 참조할 수 있지만, 해당 스프라이트는 높이와 너비가 같은 다른 스프라이트가 있는 이미지 디렉터리에 저장해야 합니다.

`"id"`에는 같은 스프라이트를 공유하는 여러 게임 엔티티의 목록도 지정할 수 있습니다(예: `"id": ["vp_door", "vp_hddoor"]`). `"id"`에는 게임의 차량 부품, 지형, 가구, 아이템, 몬스터를 지정할 수 있습니다. 특수 ID `"player_female"`, `"player_male"`, `"npc_female"`, `"npc_male"`는 플레이어 아바타와 NPC 스프라이트를 식별합니다. 특수 ID `"unknown"`은 다른 스프라이트가 없는 엔티티에 표시할 스프라이트를 제공합니다.

특수 접미사 `_season_spring`, `_season_summer`, `_season_autumn`, `_season_winter`를 엔티티 ID에 붙이면 해당 계절에 표시할 계절 변형을 만들 수 있습니다. 예: `"id": "mon_wolf_season_winter"`.

특수 접두사 `overlay_mutation_`, `overlay_female_mutation_`, `overlay_male_mutation_`를 게임의 특성이나 바이오닉 앞에 붙이면 해당 돌연변이나 바이오닉을 나타내도록 플레이어와 NPC 스프라이트 위에 그릴 오버레이 이미지를 지정할 수 있습니다.

특수 접두사 `overlay_worn_`, `overlay_female_worn_`, `overlay_male_worn_`를 게임의 아이템 앞에 붙이면 해당 아이템을 착용했음을 나타내도록 플레이어와 NPC 스프라이트 위에 그릴 오버레이 이미지를 지정할 수 있습니다.

특수 접두사 `overlay_wielded_`, `overlay_female_wielded_`, `overlay_male_wielded_`를 게임의 아이템 앞에 붙이면 해당 아이템을 들고 있음을 나타내도록 플레이어와 NPC 스프라이트 위에 그릴 오버레이 이미지를 지정할 수 있습니다.

`"fg"`와 `"bg"`에는 미리 회전한 변형 2개 또는 4개의 목록도 지정할 수 있습니다. 예: `"bg": ["t_wall_n", "t_wall_e", "t_wall_s", "t_wall_w"]` 또는 `"fg": ["mon_dog_left", "mon_dog_right"]`.

`"fg"`와 `"bg"`에는 가중치에 따라 무작위로 고르는 선택지 딕셔너리 목록도 지정할 수 있으며, 각 선택지는 회전 목록일 수도 있습니다.

```cpp
"fg": [
    { "weight": 50, "sprite": "t_dirt_brown"},       // 53개 중 50개 타일에 나타남
    { "weight": 1, "sprite": "t_dirt_black_specks"}, // 53개 중 1개 타일에 나타남
    { "weight": 1, "sprite": "t_dirt_specks_gray"},
    { "weight": 1, "sprite": "t_patchy_grass"}       // 파일명은 자유롭게 지정 가능
],
```

`"multitile"`은 선택 필드입니다. 이 필드가 있고 값이 `true`이면, 아이템의 파손 상태나 벽 연결부처럼 이 타일과 연관된 엔티티 및 스프라이트의 딕셔너리가 하나 이상 든 `additional_tiles` 목록이 있어야 합니다. 목록의 각 딕셔너리에는 위와 같은 `"id"` 필드와 `"fg"` 필드가 있으며, `"fg"`에는 파일명 하나, 파일명 목록, 또는 위와 같은 딕셔너리 목록을 지정할 수 있습니다.

각 `tile_entry.json` 파일에는 객체 하나 또는 다음과 같이 하나 이상의 객체 목록을 넣을 수 있습니다.

```cpp
[
    { "id": "mon_zombie", "fg": "mon_zombie", "bg": "mon_zombie_bg", "rotates": false },
    { "id": "corpse_mon_zombie", "fg": "mon_zombie_corpse", "bg": "mon_zombie_bg", "rotates": false },
    { "id": "overlay_wielding_corse_mon_zombie", "fg": "wielded_mon_zombie_corpse", "bg": [], "rotates": false }
]
```

파일에 타일 엔트리 목록을 두면 정리에 유용할 수 있지만, 서로 전혀 관계없는 엔트리를 같은 파일에 넣어도 문제가 없습니다.

#### 확장 `tile_entry` JSON

타일시트에는 모드가 제공하는 확장 타일시트를 둘 수 있습니다. 각 확장 타일시트는 단일 `"id"` 값, `"rotates": false`, `"fg": 0`으로 구성됩니다. 확장 `tile_entry` JSON만 `"fg"`에 정수를 사용하며 그 값은 반드시 0이어야 합니다. 확장 `tile_entry` JSON은 각 이미지 디렉터리의 최상위에 있어야 합니다.

#### 스프라이트 이미지

이미지 디렉터리 안의 모든 스프라이트는 그 디렉터리의 다른 모든 스프라이트와 높이와 너비가 같아야 합니다.

스프라이트는 타일셋 개발자가 원하는 방식으로 이미지 디렉터리의 하위 디렉터리에 정리할 수 있습니다. 스프라이트 파일명에는 아무 제한이 없으며 개발자에게 적합한 명명 체계를 사용하면 됩니다.

타일셋을 불러온 뒤 `config/debug.log`에는 그 타일셋에서 스프라이트가 누락된 모든 엔티티가 공백으로 구분된 목록으로 기록됩니다. `"looks_like"` 정의 덕분에 스프라이트가 있는 엔티티는 목록에 나오지 않습니다.

### `tile_info.json`

각 합성형 타일셋에는 다음 형태의 `tile_info.json`이 반드시 있어야 합니다.

```
[
  {
    "width": 32,
    "pixelscale": 1,
    "height": 32
  },
  {
    "1_tiles_32x32_0-5199.png": {}
  },
  {
    "2_expan_32x32_5200-5391.png": {}
  },
  {
    "3_tree_64x80_5392-5471.png": {
      "sprite_offset_x": -16,
      "sprite_offset_y": -48,
      "sprite_height": 80,
      "sprite_width": 64
    }
  },
  {
    "4_fallback_5472-9567.png": { "fallback": true }
  }
]
```

첫 딕셔너리는 필수이며 타일셋의 모든 타일시트에 적용할 기본 스프라이트 너비와 높이를 지정합니다. 각 이미지 디렉터리마다 타일시트 PNG 이름을 키로 하는 별도의 딕셔너리가 있어야 합니다. 타일시트가 기본 스프라이트 크기를 사용하고 특별한 오프셋이 없다면 타일시트 이름 키의 값은 빈 딕셔너리여도 됩니다. 그렇지 않으면 스프라이트 오프셋, 높이, 너비를 딕셔너리에 지정해야 합니다.

`"fallback"`은 특수 키이며, 있다면 `true`여야 합니다. fallback으로 지정한 타일시트는 대체 ASCII 문자 타일시트로 취급합니다. `scripts/tileset.ts`는 fallback 타일시트를 타일셋 끝에 합성하며, `tile_info.json`에 `"fallback"` 엔트리가 없으면 `tile_config.json`에 `"fallback.png"`를 추가합니다.

`"filler"`도 특수 키이며, 있다면 `true`여야 합니다. filler로 지정한 타일시트 디렉터리의 엔트리는 비-filler 디렉터리에서 같은 ID를 이미 정의했거나 그 filler 디렉터리에서 ID를 이미 정의했으면 무시됩니다. filler 디렉터리의 PNG도 비-filler 디렉터리의 PNG와 이름이 같으면 무시됩니다. filler 타일시트는 타일셋 아트를 개선할 때 유용합니다. 오래된 저품질 아트를 filler 타일시트에 두면 비-filler 타일시트에 더 나은 이미지가 추가되는 대로 자동으로 교체됩니다.

## 레거시 타일셋

### 타일시트

각 타일시트에는 너비와 높이가 같은 스프라이트가 하나 이상 있습니다. 각 타일시트는 한 행에 정확히 16개 스프라이트가 있는 행을 하나 이상 포함합니다. 스프라이트 인덱스 0은 특별하므로 타일셋 첫 타일시트의 첫 스프라이트는 비워 두어야 합니다. 인덱스는 각 시트에 걸쳐 연속되며 새 시트에서도 초기화되지 않습니다. 따라서 인덱스 32는 첫 시트 셋째 행의 첫 스프라이트입니다. 첫 시트에 스프라이트가 320개 있다면 인덱스 352는 둘째 시트 셋째 행의 첫 스프라이트입니다.

### `tile_config`

각 레거시 타일셋에는 스프라이트 시트의 내용을 여러 타일 식별자와 방향 등에 매핑하는 방법을 설명하는 `tile_config.json`이 있습니다. 돌연변이를 표시할 때 사용하는 오버레이 순서도 제어할 수 있으며, 이 순서로 `mutation_ordering.json`의 기본 순서를 덮어쓸 수 있습니다. 예:

```json
{ // 파일 전체가 단일 객체
  "tile_info": [ // tile_info는 필수
    {
      "height": 32,
      "width": 32,
      "iso": true, //  선택 사항. 아이소메트릭 타일셋임을 나타냅니다. 기본값은 false.
      "pixelscale": 2 //  선택 사항. 타일셋 크기 조절 배수를 설정합니다. 기본값은 1.
    }
  ],
  "tiles-new": [ // tiles-new는 스프라이트 시트 배열
    { //   또는 "tiles" 배열 하나만 사용
      "file": "tiles.png", // 격자 형태로 스프라이트가 든 파일
      "tiles": [ // 타일당 엔트리 하나인 배열
        {
          "id": "10mm", // 게임 엔티티를 스프라이트에 매핑하는 ID
          "fg": 1, //   접두사가 없으면 대부분 아이템
          "bg": 632, // fg와 bg는 이미지의 스프라이트 인덱스일 수 있음
          "rotates": false
        },
        {
          "id": "t_wall", // "t_"는 지형
          "fg": [2918, 2919, 2918, 2919], // 스프라이트 번호 2개나 4개는 미리 회전됨
          "bg": 633,
          "rotates": true,
          "multitile": true,
          "additional_tiles": [ // 연결/조합된 스프라이트 버전
            { //   또는 변형. 아래 참조
              "id": "center",
              "fg": [2919, 2918, 2919, 2918]
            },
            {
              "id": "corner",
              "fg": [2924, 2922, 2922, 2923]
            },
            {
              "id": "end_piece",
              "fg": [2918, 2919, 2918, 2919]
            },
            {
              "id": "t_connection",
              "fg": [2919, 2918, 2919, 2918]
            },
            {
              "id": "unconnected",
              "fg": 2235
            }
          ]
        },
        {
          "id": "vp_atomic_lamp", // "vp_"는 차량 부품
          "fg": 3019,
          "bg": 632,
          "rotates": false,
          "multitile": true,
          "additional_tiles": [
            {
              "id": "broken", // 변형 스프라이트
              "fg": 3021
            }
          ]
        },
        {
          "id": "t_dirt",
          "rotates": false,
          "fg": [
            { "weight": 50, "sprite": 640 }, // 가중 무작위 변형
            { "weight": 1, "sprite": 3620 },
            { "weight": 1, "sprite": 3621 },
            { "weight": 1, "sprite": 3622 }
          ]
        },
        {
          "id": [
            "overlay_mutation_GOURMAND", // 돌연변이 캐릭터 오버레이
            "overlay_mutation_male_GOURMAND", // 특정 성별 오버레이
            "overlay_mutation_active_GOURMAND" // 활성 돌연변이 오버레이
          ],
          "fg": 4040
        }
      ]
    },
    { // tiles-new의 두 번째 엔트리
      "file": "moretiles.png", // 다른 스프라이트 시트
      "tiles": [
        {
          "id": ["xxx", "yyy"], // ID 두 개를 한 번에 정의
          "fg": 1,
          "bg": 234
        }
      ]
    }
  ],
  "overlay_ordering": [
    {
      "id": "WINGS_BAT", // 문자열 또는 문자열 배열인 돌연변이 이름
      "order": 1000 // 범위는 0~9999이며 9999가 가장 위 레이어
    },
    {
      "id": ["PLANTSKIN", "BARK"], // 문자열 또는 문자열 배열인 돌연변이 이름
      "order": 3500 // 배열의 모든 항목에 순서 적용
    },
    {
      "id": "bio_armor_torso", // 바이오닉 오버레이 순서도 같은 방식으로 제어
      "order": 500
    }
  ]
}
```

## 색조 적용

타일셋은 색조와 색조 쌍을 지원할 수 있습니다.

### 색조 쌍

색조 쌍은 한 `type`이 다른 타일의 `type`에 따라 색조를 제어하도록 합니다. 예를 들어 `hair_color`로 `hair_style`을 제어할 수 있습니다.

```json
"tint_pairs": [
  { "source_type": "hair_color", "target_type": "hair_style", "override": true },
  { "source_type": "hair_color", "target_type": "facial_hair", "override": true }
],
"tints": [
			{ "id": "hair_blond", "fg": "#91631f", "contrast": 1.1, "blend_mode": "multiply" },
			{ "id": "hair_white", "fg": "#ffffff", "blend_mode": "multiply" },
      //...
],
"tiles-new": [//...
```

`override`의 기본값은 false이며 레거시 타일 지정을 우회하게 합니다. 이는 메인 타일셋의 엔트리를 제거할 수 없는 `mod_tileset`에 더 유용합니다. `source_type`은 오버레이의 종류에 따라 `target_type`을 제어합니다. 현재는 돌연변이에만 영향을 주므로 입력으로 `mutation_type`을 처리할 수 있습니다. 또는 `target_type`이 태그와 일치하게 할 수 있어, 머리 색에 따라 털 색조를 바꾸는 등의 작업이 가능합니다.

### 색조

색조를 사용하면 단순한 색상 변형마다 별도의 스프라이트를 만들 필요 없이 타일 색상을 수정할 수 있습니다. 색조 엔트리의 `id`는 타일이나 색조 쌍의 소스를 가리킬 수 있습니다. 예:

```json
"tint_pairs": [//...
"tints": [
  { "id": "eye_pink", "fg": "#ff00bb", "saturation": 1.5 },
  { "id": "eye_black", "fg": "c_black", "blend_mode": "multiply" },
  { "id": "eye_white", "fg": { "color": "#ffffff", "saturation": 0.0, "brightness": 1.2 } },
],
"tiles-new": [//...
```

색조는 매우 유연합니다. `fg`와 `bg`를 따로 처리할 수 있습니다. 색상 입력이나 색상과 수정자가 든 엔트리 중 하나를 사용할 수 있으며, 두 형식을 섞을 수는 없습니다. 전경과 배경 양쪽에 적용되는 수정자를 사용하면서 엔트리 방식도 함께 사용할 수 없습니다. 수정자는 `saturation`, `brightness`, `contrast`입니다. 이 기능도 주로 `mod_tileset`에 유용하지만 거기에만 한정되지는 않습니다. 또한 `blend_mode`에 다음 값 중 하나를 지정할 수 있습니다.

- `tint` (기본값)
- `overlay`
- `softlight`
- `hardlight`
- `multiply`
- `additive`
- `subtract`
- `normal`
- `screen`
- `divide`

참고 자료:
https://en.wikipedia.org/wiki/Blend_modes

특히 `tint`는 이 기능 전용 모드입니다. 검정색과 흰색도 결과 타일의 색을 완전히 바꾸면서 적절한 대비를 유지하므로 칠한 듯한 효과에 유용합니다. `normal`은 색조의 알파를 사용하면 효과 주위에 어색한 사각형 테두리가 생기므로 현재 타일의 알파를 사용합니다.

색상에는 16진수 코드나 curses 색상 이름을 사용할 수 있습니다. ID를 이용해 curses 색상을 얻는 대체 로직도 있지만 의존해서는 안 됩니다. 색조는 현재 돌연변이, 아이템, 바이오닉, 효과에 적용할 수 있습니다. ID나 태그로 색조를 지정할 수 있지만 효과 플래그는 지원하지 않습니다.

## 투사체 스프라이트

특정 명명 규칙을 사용하여 투사체(탄환과 투척 아이템)의 사용자 지정 스프라이트를 정의할 수 있습니다.

> [!NOTE]
> 올바른 방향으로 표시하려면 스프라이트가 위쪽(0도)을 향해야 합니다.

### 탄환(총에서 발사)

`animation_bullet_{ammo_type}`을 사용하며 `{ammo_type}`은 탄약의 아이템 ID입니다.

```json
{ "id": "animation_bullet_9mm", "fg": 123, "rotates": true }
{ "id": "animation_bullet_556", "fg": 124, "rotates": true }
{ "id": "animation_bullet_762", "fg": 125, "rotates": true }
```

시스템은 탄약의 `looks_like` 체인을 따릅니다. `animation_bullet_556`이 없지만 `556` 탄약에 `looks_like: "223"`이 있으면, `animation_bullet_223`이 있을 때 자동으로 사용합니다.

### 투척 아이템

`animation_bullet_{item_type}`을 사용하며 `{item_type}`은 투척 아이템의 ID입니다.

```json
{ "id": "animation_bullet_javelin", "fg": 126, "rotates": true }
{ "id": "animation_bullet_throwing_axe", "fg": 127, "rotates": true }
{ "id": "animation_bullet_throwing_knife", "fg": 128, "rotates": true }
```

투척 아이템도 아이템의 `looks_like` 체인을 따릅니다.

### 대체 동작

사용자 지정 투사체 스프라이트를 찾지 못하면 다음과 같이 대체합니다.

1. **투척 아이템**: 해당 아이템 자체의 스프라이트(예: `javelin`)
2. **탄환**: `animation_bullet_normal_0deg`

### 회전

- 재블린과 창처럼 `FLY_STRAIGHT` 플래그가 있는 아이템은 비행 중 방향을 유지합니다.
- 도끼, 칼 등의 다른 투척 아이템은 비행 중 회전합니다.
- 타일 정의에 `"rotates": true`를 설정하여 방향별 스프라이트 지원을 활성화합니다.

<a id="state-modifiers"></a>

## 상태 수정자

상태 수정자는 상태마다 별도의 그림을 만들지 않고 게임 상태(웅크림, 쓰러짐 등)에 따라 캐릭터 스프라이트를 동적으로 조정합니다. 수정자 이미지가 픽셀 변위를 제어하는 UV 매핑을 사용합니다.

### UV 매핑 방식

UV 매핑은 2D 이미지로 다른 이미지의 샘플링 방식을 제어하는 3D 그래픽 기법입니다. 여기서는 다음과 같이 작동합니다.

- UV 수정자 이미지의 각 픽셀은 빨간색(X) 및 녹색(Y) 채널로 변위를 인코딩합니다.
- 렌더링할 때 픽셀 `(x, y)`를 직접 그리지 않고 수정자의 `(x, y)`를 읽어 원본 스프라이트에서 샘플링할 위치를 결정합니다.
- 따라서 스프라이트 일부를 압축하거나 늘리거나 이동할 수 있습니다.

### 오프셋 모드와 정규화 모드

상태 수정자는 UV 데이터를 해석하는 두 가지 모드를 지원합니다.

**오프셋 모드**(`"use_offset": true`, 기본값):

- 빨강/녹색 값은 중립값 `(127, 127)`에 대한 변위를 나타냅니다.
- 127은 이동 없음, 0은 -127픽셀, 255는 +128픽셀입니다.
- 변경하지 않을 곳을 회색 `(127,127)`으로 칠하면 되므로 이해하기 쉽습니다.
- 여러 수정자의 변위가 더해집니다.

**정규화 모드**(`"use_offset": false`):

- 빨강/녹색 값은 타일 크기로 정규화한 절대 UV 좌표입니다.
- `(0,0)`은 왼쪽 아래, `(255,255)`는 오른쪽 위를 샘플링합니다.
- 복잡한 재매핑에는 더 정확하지만 직관적이지 않습니다.
- UV를 회전하면 결과도 회전하므로 빠르게 수정하기 쉽습니다.
- 수정자는 서로를 다시 샘플링하며 연결됩니다.
- 계산 비용이 약간 더 큽니다.

### JSON 구조

상태 수정자는 타일셋의 타일 설정에 있는 `"state-modifiers"` 배열 안에 정의합니다.

```json
"state-modifiers": [
  {
    "id": "movement_mode",
    "override": false,
    "use_offset": true,
    "tiles": [
      { "id": "walk", "fg": null },
      { "id": "crouch", "fg": 100 },
      { "id": "run", "fg": 101 }
    ]
  },
  {
    "id": "downed",
    "override": true,
    "use_offset": true,
    "tiles": [
      { "id": "normal", "fg": null },
      { "id": "downed", "fg": 102 }
    ]
  }
]
```

### 필드

| 필드         | 타입   | 설명                                                                      |
| ------------ | ------ | ------------------------------------------------------------------------- |
| `id`         | string | 수정자 그룹 식별자. 아래의 지원 그룹과 일치해야 합니다.                   |
| `override`   | bool   | `true`이고 이 상태가 활성화되면 우선순위가 낮은 그룹을 건너뜁니다.        |
| `use_offset` | bool   | 오프셋 모드는 `true`, 정규화 모드는 `false`입니다. 기본값은 `true`입니다. |
| `tiles`      | array  | 이 그룹의 상태-스프라이트 매핑입니다.                                     |
| `whitelist`  | array  | 선택 사항. 이 접두사와 일치하는 오버레이에만 적용합니다.                  |
| `blacklist`  | array  | 선택 사항. 이 접두사와 일치하는 오버레이에는 적용하지 않습니다.           |

`tiles`의 각 항목:

| 필드     | 타입     | 설명                                                                           |
| -------- | -------- | ------------------------------------------------------------------------------ |
| `id`     | string   | 그룹 안의 상태 식별자입니다.                                                   |
| `fg`     | int/null | UV 수정자 이미지의 스프라이트 인덱스입니다. `null`은 수정하지 않음을 뜻합니다. |
| `offset` | object   | 선택 사항. 대형 수정자 스프라이트에 쓰는 `{"x": n, "y": n}`입니다.             |

### 지원되는 수정자 그룹

| 그룹 ID         | 상태                                       | 설명                                   |
| --------------- | ------------------------------------------ | -------------------------------------- |
| `movement_mode` | `walk`, `run`, `crouch`                    | 캐릭터의 이동 자세                     |
| `downed`        | `normal`, `downed`                         | 캐릭터가 쓰러졌는지 여부               |
| `lying_down`    | `normal`, `lying`                          | 캐릭터가 누워 있는지 여부(수면 등)     |
| `activity`      | `none`, 활동 ID                            | 현재 활동(예: `ACT_CRAFT`, `ACT_READ`) |
| `body_size`     | `tiny`, `small`, `medium`, `large`, `huge` | 돌연변이로 바뀐 캐릭터 크기            |

### 우선순위와 덮어쓰기

수정자 그룹은 배열 순서대로 처리하며 인덱스 0의 우선순위가 가장 높습니다. 그룹에 `"override": true`가 설정되어 있고 해당 상태에 활성 수정자(`fg`가 null이 아님)가 있으면, 우선순위가 낮은 그룹을 모두 건너뜁니다. 예를 들어 쓰러진 상태가 이동 기반 수정을 완전히 대체하도록 할 수 있습니다.

### 오버레이 필터

그룹별 `whitelist`와 `blacklist` 배열은 수정자가 영향을 줄 오버레이를 제어합니다. 오버레이는 접두사로 비교합니다(예: `"wielded_"`는 `"wielded_katana"`와 일치). 그룹이 필터 중 하나라도 지정하면 전역 필터를 덮어씁니다. 흔한 접두사는 `wielded_`, `worn_`, `mutation_`, `effect_`, `bionic_`입니다.

```json
{
  "id": "movement_mode",
  "blacklist": ["wielded_"],
  "tiles": [...]
}
```

필터가 서로 다르면 같은 `id`를 가진 그룹을 여러 개 둘 수 있어 오버레이 유형마다 다른 UV 수정자를 사용할 수 있습니다. 이때 **필터는 서로 배타적이어야** 하며 각 오버레이가 ID별로 최대 한 그룹과 일치해야 합니다. 필터가 겹치면 중복 렌더링 문제가 생깁니다.

```json
{
  "id": "movement_mode",
  "blacklist": ["wielded_"],
  "tiles": [...]
},
{
  "id": "movement_mode",
  "whitelist": ["wielded_"],
  "tiles": [...]
}
```

**기본 스프라이트 동작:** 기본 캐릭터 스프라이트(피부, 눈, 머리카락 등)는 오버레이가 아니므로 접두사가 없습니다. 접두사와 일치할 수 없어서 `whitelist`가 있는 그룹은 기본 스프라이트에 적용되지 않습니다. `blacklist`만 있거나 필터가 없는 그룹은 기본 스프라이트에 정상적으로 적용됩니다. 기본 스프라이트와 특정 오버레이에 서로 다른 수정자를 적용하려면 기본 스프라이트용 blacklist 그룹과 오버레이용 whitelist 그룹을 사용하세요.

### UV 수정자 스프라이트 만들기

#### 방법 1: use_offset = true

1. 중립 회색 이미지(RGBA 127, 127, 0, 255)에서 시작합니다.
2. 빨강 채널로 픽셀의 가로 이동을 지정합니다(127 미만은 왼쪽, 127 초과는 오른쪽).
3. 녹색 채널로 픽셀의 세로 이동을 지정합니다(127 미만은 위, 127 초과는 아래).

웅크림 효과를 만들려면 수정자의 아래쪽을 127 미만의 녹색 값으로 칠해 픽셀을 아래로 끌어내리고 스프라이트를 세로로 압축할 수 있습니다.

---

#### 방법 2: use_offset = false

1. 기본 UV Identity 이미지에서 시작합니다.

<img src="./img/uv_identity.png" width="128" height="128">

2. 이동, 회전, 크기 조절 등에 따라 픽셀을 옮겨 결과에 해당 효과를 만듭니다.
3. 녹색 채널로 픽셀의 세로 이동을 지정합니다(127 미만은 위, 127 초과는 아래).

이 방식은 직관적이지 않을 수 있으므로 다음 예를 참고하세요.

<details><summary>서 있는 상태</summary>

서 있는 상태가 정상 상태이므로 UV 이미지를 수정하지 않습니다.

<img src="./img/uv_identity.png" width="256" height="256">
<img src="./img/uv_identity_result.png" width="256" height="256">
</details>

<details><summary>웅크린 상태</summary>

눈에 잘 띄지 않을 수 있지만 캐릭터 바로 아래의 픽셀을 조정했고 맨 위 픽셀은 사라졌습니다. UV 대부분을 아래로 내렸습니다.

<img src="./img/uv_crouch.png" width="256" height="256">
<img src="./img/uv_crouch_result.png" width="256" height="256">
</details>
</details>

<details><summary>누운 상태</summary>

아주 간단합니다. 등을 더 평평하게 눕히는 작은 조정이 있지만, 대체로 UV 이미지 전체를 회전하고 약간 아래로 옮긴 것입니다.

<img src="./img/uv_lying_down.png" width="256" height="256">
<img src="./img/uv_lying_down_result.png" width="256" height="256">
</details>

---

파란색 채널은 무시되며 알파값 0은 픽셀을 투명하게 만듭니다.

### 스프라이트 크기

타일셋을 정의할 때 `sprite_width`, `sprite_height`와 함께 `sprite_offset_x`, `sprite_offset_y`를 지정합니다. UV에서는 이 값이 영향을 미칠 영역을 정의하며 경계 밖의 픽셀은 바뀌지 않습니다. 모든 스프라이트의 해상도가 같다면 중요하지 않지만, 오버레이는 오프셋이나 크기가 다를 수도 있습니다. 이 기능은 픽셀 배율이 일정하다고 가정하지만 그 외에는 크기가 다른 스프라이트를 지원합니다.

무기나 상태 표시처럼 아바타 바깥까지 뻗는 오버레이를 처리하려면 상태 수정자를 64x64, 오프셋 `(-16,-16)`으로 제작하기를 권장합니다. 이론적으로 상태 수정자의 메모리 사용량을 줄일 수도 있습니다. 예를 들어 표정을 새 수정자 그룹으로 구현한다면 아주 작은 UV를 사용하여 얼굴 영역만 수정할 수 있습니다. 이 경우 반올림 문제를 피하려면 오프셋 모드를 사용하는 편이 좋습니다.

### 스프라이트 경계

원래 스프라이트 경계 밖으로 이동한 픽셀도 지원하지만 렌더링에 한계가 있습니다. 타일은 위에서 아래로 행 단위로 그리므로 왜곡된 스프라이트가 아래 행으로 넘어가면 다음 행의 지형을 그릴 때 그 부분이 덮어써집니다. 위, 왼쪽, 오른쪽으로 확장되는 스프라이트는 정상적으로 렌더링됩니다. 이는 타일 렌더링 순서의 근본적인 한계입니다.

### 성능

상태 수정자는 렌더링할 때 처리되며 고유한 상태 조합별로 캐시됩니다. 성능이 문제가 되면 그래픽 옵션의 `State Modifiers` 토글로 이 기능을 끌 수 있습니다.

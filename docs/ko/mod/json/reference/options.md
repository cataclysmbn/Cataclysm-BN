# 옵션

## 옵션 그룹

옵션 메뉴에 새 그룹을 추가합니다.

| 키      | 설명                                                                                              |
| ------- | ------------------------------------------------------------------------------------------------- |
| `type`  | 항상 `OPTION_GROUP`입니다.                                                                        |
| `id`    | 그룹의 ID입니다.                                                                                  |
| `page`  | `general`, `interface`, `graphics`, `performance`, `world_default`, `debug` 또는 `android`입니다. |
| `title` | 표시되는 옵션 이름입니다.                                                                         |
| `desc`  | 확장된 옵션 설명입니다.                                                                           |

## 옵션 공간

옵션 메뉴에 새 공간을 추가합니다.

| 키     | 설명                                                                                              |
| ------ | ------------------------------------------------------------------------------------------------- |
| `type` | 항상 `OPTION_SPACE`입니다.                                                                        |
| `page` | `general`, `interface`, `graphics`, `performance`, `world_default`, `debug` 또는 `android`입니다. |

## 옵션

옵션 메뉴에 옵션을 추가합니다.

| 키                | 설명                                                                                                                                                                |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `type`            | 항상 `OPTION`입니다.                                                                                                                                                |
| `id`              | 고유한 옵션 ID입니다.                                                                                                                                               |
| `stype`           | 옵션의 유형입니다.                                                                                                                                                  |
| `page`            | `general`, `interface`, `graphics`, `performance`, `world_default`, `debug` 또는 `android`입니다.                                                                   |
| `group`           | 옵션이 속할 선택적 그룹 ID입니다.                                                                                                                                   |
| `menu_text`       | 옵션 값에 표시할 텍스트입니다.                                                                                                                                      |
| `tooltip`         | 옵션에 표시할 추가 정보입니다.                                                                                                                                      |
| `default`         | 기본값입니다.                                                                                                                                                       |
| `default_android` | Android에서의 기본값입니다.                                                                                                                                         |
| `min`             | 정수/실수의 최솟값입니다.                                                                                                                                           |
| `max`             | 정수/실수의 최댓값입니다.                                                                                                                                           |
| `step`            | 메뉴에서 실수에 사용할 간격입니다.                                                                                                                                  |
| `hide`            | `none`, `always`, `sdl_hide`(SDL 사용 시), `curses_hide`(curses 사용 시), `no_sound_hide`(사운드가 없을 때), `android_only`(Android가 아닐 때)로 숨김을 지정합니다. |
| `deps`            | 설정에 사용할 `option`(다른 옵션 ID)과 `values`(유효한 옵션 값)의 배열입니다.                                                                                       |

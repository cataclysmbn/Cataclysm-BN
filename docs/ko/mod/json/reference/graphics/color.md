# 색상

BN은 다채로운 게임입니다. 다음과 같은 여러 곳에서 전경색과 배경색을 사용할 수 있습니다.

- 맵 데이터(지형과 가구)
- 아이템 데이터
- 텍스트 데이터
- 기타

**참고:** 맵 데이터 객체에는 색상 관련 노드를 하나만 정의할 수 있습니다(`color` 또는 `bgcolor`).

## 색상 문자열 형식

JSON에서 색상을 정의할 때는 다음 형식을 사용합니다: `Prefix_Foreground_Background`.

`Prefix`에는 다음 값을 사용할 수 있습니다.

- `c_` - 기본 색상 접두사(생략 가능)
- `i_` - 전경색을 반전한다는 선택적 접두사(전경색과 배경색에 특별 규칙 적용)
- `h_` - 전경색을 강조한다는 선택적 접두사(전경색과 배경색에 특별 규칙 적용)

`Foreground`는 전경/잉크/글꼴의 필수 색상을 지정합니다.

`Background`는 선택적 배경/종이 색상을 지정합니다.

**참고:** 모든 전경색+배경색 조합이 전체 이름으로 정의되어 있지는 않습니다. 모든 색상 이름은 게임 내 색상 관리자에서 확인하세요.

**참고:** 이름으로 색상을 찾지 못하면 `Foreground`에는 `c_unset`, `Background`에는 `i_white`가 사용됩니다.

## 색상 문자열 예시

- `c_white` - `c_` 기본 접두사를 사용하는 `white` 색상
- `black` - 기본 접두사 `c_`를 생략한 `black` 색상
- `i_red` - 반전된 `red` 색상
- `dark_gray_white` - 전경색 `dark_gray`, 배경색 `white`
- `light_gray_light_red` - 전경색 `light_gray`, 배경색 `light_red`
- `dkgray_red` - `dark_` 대신 사용되는 더 이상 권장되지 않는 `dk` 접두사의 `dark_gray` 전경색과 `red` 배경색
- `ltblue_red` - `light_` 대신 사용되는 더 이상 권장되지 않는 `lt` 접두사의 `light_blue` 전경색과 `red` 배경색

## 색상 코드

색상 코드는 색상을 정의하는 짧은 문자열이며, 예를 들어 맵 메모에서 사용할 수 있습니다.

## 가능한 색상

|                       색상(이미지)                       | 색상 이름(cataclysm)  | 색상 이름(curses) | 기본 R,G,B 값 | 색상 코드 |                    비고                    |
| :------------------------------------------------------: | :-------------------: | :---------------: | :-----------: | :-------: | :----------------------------------------: |
| ![#000000](https://placehold.it/20/000000/000000?text=+) |        `black`        |      `BLACK`      |    `0,0,0`    |           |                                            |
| ![#ff0000](https://placehold.it/20/ff0000/000000?text=+) |         `red`         |       `RED`       |   `255,0,0`   |    `R`    |                                            |
| ![#006e00](https://placehold.it/20/006e00/000000?text=+) |        `green`        |      `GREEN`      |   `0,110,0`   |    `G`    |                                            |
| ![#5c3317](https://placehold.it/20/5c3317/000000?text=+) |        `brown`        |      `BROWN`      |  `92,51,23`   |   `br`    |                                            |
| ![#0000c8](https://placehold.it/20/0000c8/000000?text=+) |        `blue`         |      `BLUE`       |   `0,0,200`   |    `B`    |                                            |
| ![#8b3a62](https://placehold.it/20/8b3a62/000000?text=+) | `magenta` 또는 `pink` |     `MAGENTA`     |  `139,58,98`  |    `P`    |                                            |
| ![#0096b4](https://placehold.it/20/0096b4/000000?text=+) |        `cyan`         |      `CYAN`       |  `0,150,180`  |    `C`    |                                            |
| ![#969696](https://placehold.it/20/969696/000000?text=+) |     `light_gray`      |      `GRAY`       | `150,150,150` |   `lg`    | `light_` 대신 폐기된 `lt` 접두사 사용 가능 |
| ![#636363](https://placehold.it/20/636363/000000?text=+) |      `dark_gray`      |      `DGRAY`      |  `99,99,99`   |   `dg`    | `dark_` 대신 폐기된 `dk` 접두사 사용 가능  |
| ![#ff9696](https://placehold.it/20/ff9696/000000?text=+) |      `light_red`      |      `LRED`       | `255,150,150` |           | `light_` 대신 폐기된 `lt` 접두사 사용 가능 |
| ![#00ff00](https://placehold.it/20/00ff00/000000?text=+) |     `light_green`     |     `LGREEN`      |   `0,255,0`   |    `g`    | `light_` 대신 폐기된 `lt` 접두사 사용 가능 |
| ![#ffff00](https://placehold.it/20/ffff00/000000?text=+) |    `light_yellow`     |     `YELLOW`      |  `255,255,0`  |           | `light_` 대신 폐기된 `lt` 접두사 사용 가능 |
| ![#6464ff](https://placehold.it/20/6464ff/000000?text=+) |     `light_blue`      |      `LBLUE`      | `100,100,255` |    `b`    | `light_` 대신 폐기된 `lt` 접두사 사용 가능 |
| ![#fe00fe](https://placehold.it/20/fe00fe/000000?text=+) |    `light_magenta`    |    `LMAGENTA`     |  `254,0,254`  |   `lm`    | `light_` 대신 폐기된 `lt` 접두사 사용 가능 |
| ![#00f0ff](https://placehold.it/20/00f0ff/000000?text=+) |     `light_cyan`      |      `LCYAN`      |  `0,240,255`  |    `c`    | `light_` 대신 폐기된 `lt` 접두사 사용 가능 |
| ![#ffffff](https://placehold.it/20/ffffff/000000?text=+) |        `white`        |      `WHITE`      | `255,255,255` |    `W`    |                                            |

**참고:** 기본 RGB 값은 `\data\raw\colors.json`에서 가져옵니다. **참고:** RGB 값은 `\config\base_colors.json`에서 재정의할 수 있습니다.

## 색상 규칙

전경색과 배경색 모두에 영향을 줄 수 있는 특별한 색상 변환은 두 가지입니다.

- 반전
- 강조

**참고:** 색상 규칙은 재정의할 수 있습니다(예: `\data\raw\color_templates\no_bright_background.json`).

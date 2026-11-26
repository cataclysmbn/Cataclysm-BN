# 色

BN は色彩豊かなゲームです。次のようなさまざまな場所で前景色と背景色を使用できます。

- マップデータ（地形と家具）
- アイテムデータ
- テキストデータ
- その他

**注記:** マップデータオブジェクトで定義できる色関連ノードは 1 つだけです（`color` または `bgcolor`）。

## 色文字列の形式

JSON で色を定義するときは、次の形式を使用します: `Prefix_Foreground_Background`。

`Prefix` には次の値を指定できます。

- `c_` - デフォルトの色接頭辞（省略可能）
- `i_` - 前景色を反転することを示すオプションの接頭辞（前景色と背景色に特殊規則を適用）
- `h_` - 前景色を強調することを示すオプションの接頭辞（前景色と背景色に特殊規則を適用）

`Foreground` は前景/インク/フォントの必須色を指定します。

`Background` はオプションの背景/用紙色を指定します。

**注記:** すべての前景色と背景色の組み合わせに完全な名前が定義されているわけではありません。すべての色名はゲーム内の色マネージャーで確認してください。

**注記:** 名前で色が見つからない場合、`Foreground` には `c_unset`、`Background` には `i_white` が使用されます。

## 色文字列の例

- `c_white` - デフォルト接頭辞 `c_` を使用する `white` 色
- `black` - デフォルト接頭辞 `c_` を省略した `black` 色
- `i_red` - 反転した `red` 色
- `dark_gray_white` - 前景色 `dark_gray`、背景色 `white`
- `light_gray_light_red` - 前景色 `light_gray`、背景色 `light_red`
- `dkgray_red` - `dark_` の代わりに非推奨の `dk` 接頭辞を使う `dark_gray` 前景色と `red` 背景色
- `ltblue_red` - `light_` の代わりに非推奨の `lt` 接頭辞を使う `light_blue` 前景色と `red` 背景色

## 色コード

色コードは色を定義する短い文字列で、たとえばマップメモで使用できます。

## 使用可能な色

|                        色（画像）                        |    色名（cataclysm）    | 色名（curses） | デフォルト R,G,B 値 | 色コード |                       注記                        |
| :------------------------------------------------------: | :---------------------: | :------------: | :-----------------: | :------: | :-----------------------------------------------: |
| ![#000000](https://placehold.it/20/000000/000000?text=+) |         `black`         |    `BLACK`     |       `0,0,0`       |          |                                                   |
| ![#ff0000](https://placehold.it/20/ff0000/000000?text=+) |          `red`          |     `RED`      |      `255,0,0`      |   `R`    |                                                   |
| ![#006e00](https://placehold.it/20/006e00/000000?text=+) |         `green`         |    `GREEN`     |      `0,110,0`      |   `G`    |                                                   |
| ![#5c3317](https://placehold.it/20/5c3317/000000?text=+) |         `brown`         |    `BROWN`     |     `92,51,23`      |   `br`   |                                                   |
| ![#0000c8](https://placehold.it/20/0000c8/000000?text=+) |         `blue`          |     `BLUE`     |      `0,0,200`      |   `B`    |                                                   |
| ![#8b3a62](https://placehold.it/20/8b3a62/000000?text=+) | `magenta` または `pink` |   `MAGENTA`    |     `139,58,98`     |   `P`    |                                                   |
| ![#0096b4](https://placehold.it/20/0096b4/000000?text=+) |         `cyan`          |     `CYAN`     |     `0,150,180`     |   `C`    |                                                   |
| ![#969696](https://placehold.it/20/969696/000000?text=+) |      `light_gray`       |     `GRAY`     |    `150,150,150`    |   `lg`   | `light_` の代わりに非推奨の `lt` 接頭辞を使用可能 |
| ![#636363](https://placehold.it/20/636363/000000?text=+) |       `dark_gray`       |    `DGRAY`     |     `99,99,99`      |   `dg`   | `dark_` の代わりに非推奨の `dk` 接頭辞を使用可能  |
| ![#ff9696](https://placehold.it/20/ff9696/000000?text=+) |       `light_red`       |     `LRED`     |    `255,150,150`    |          | `light_` の代わりに非推奨の `lt` 接頭辞を使用可能 |
| ![#00ff00](https://placehold.it/20/00ff00/000000?text=+) |      `light_green`      |    `LGREEN`    |      `0,255,0`      |   `g`    | `light_` の代わりに非推奨の `lt` 接頭辞を使用可能 |
| ![#ffff00](https://placehold.it/20/ffff00/000000?text=+) |     `light_yellow`      |    `YELLOW`    |     `255,255,0`     |          | `light_` の代わりに非推奨の `lt` 接頭辞を使用可能 |
| ![#6464ff](https://placehold.it/20/6464ff/000000?text=+) |      `light_blue`       |    `LBLUE`     |    `100,100,255`    |   `b`    | `light_` の代わりに非推奨の `lt` 接頭辞を使用可能 |
| ![#fe00fe](https://placehold.it/20/fe00fe/000000?text=+) |     `light_magenta`     |   `LMAGENTA`   |     `254,0,254`     |   `lm`   | `light_` の代わりに非推奨の `lt` 接頭辞を使用可能 |
| ![#00f0ff](https://placehold.it/20/00f0ff/000000?text=+) |      `light_cyan`       |    `LCYAN`     |     `0,240,255`     |   `c`    | `light_` の代わりに非推奨の `lt` 接頭辞を使用可能 |
| ![#ffffff](https://placehold.it/20/ffffff/000000?text=+) |         `white`         |    `WHITE`     |    `255,255,255`    |   `W`    |                                                   |

**注記:** デフォルトの RGB 値は `\data\raw\colors.json` から取得されます。**注記:** RGB 値は `\config\base_colors.json` で再定義できます。

## 色の規則

前景色と背景色の両方に影響する特殊な色変換には、次の 2 種類があります。

- 反転
- 強調

**注記:** 色の規則は再定義できます（例: `\data\raw\color_templates\no_bright_background.json`）。

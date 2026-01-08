# 色設定について

BNは色彩豊かなゲームです。以下の場所を含む、様々な場所で複数の前景色(文字色)と背景色を使用できます:

- マップデータ (地形や設置物);
- アイテムデータ;
- テキストデータ;
- その他.

**注意:** マップデータのオブジェクトに定義できる色関連のプロパティは1つだけです (`color` または`bgcolor`のいずれか)。

## カラー文字列の形式

JSONで色を定義する場合、以下の形式で記述する必要があります。:
`Prefix_Foreground_Background`(接頭辞_前景色_背景色).

`接頭辞` 以下のいずれかの値を使用できます:

- `c_` - 既定の接頭辞です(省略可能です);
- `i_` - 反転を示す接頭辞。前景色と背景色に特殊なルールが適用されます;
- `h_` - ハイライトを示す接頭辞。前景色と背景色に特殊なルールが適用されます。

`前景色` - 文字や記号自体の色を指定します(必須)。

`背景色` - 文字の背後の色を指定します(任意)。

**注意:** すべての「前景色＋背景色」の組み合わせに個別の名称があるわけではありません。利用可能な色の全容を確認するには、ゲーム内の「カラーマネージャー」を活用してください。

**注意:** 指定した名前の色が見つからない場合、前景色には `c_unset`、`背景色` には `i_white`が自動的に割り当てられます。

## カラー文字列の例

- `c_white` - `white` 既定の接頭辞 `c_` を使用;
- `black` - `black` 接頭辞を省略;
- `i_red` - 反転した `red`;
- `dark_gray_white` - 前景色が`dark_gray` 、背景色が `white`;
- `light_gray_light_red` - 前景色が`light_gray` 背景色が `light_red`;
- `dkgray_red` - `dark_gray` 前景色が `red` (`dark_`の代わりに非推奨の接頭辞 `dk`
  を使用);
- `ltblue_red` - `light_blue` 前景色が `red` (`light_` の代わりに非推奨の接頭辞 `lt`
  を使用)。

## カラーコード

カラーコードは色を定義する略称です。主にマップノート(地図の注釈)などで使用されます。

## 利用可能な色一覧

|                       色(イメージ)                       |  色名 (cataclysm)   | 色名 (curses) | デフォルトRGB値 | コード |                   備考                   |
| :------------------------------------------------------: | :-----------------: | :-----------: | :-------------: | :----: | :--------------------------------------: |
| ![#000000](https://placehold.it/20/000000/000000?text=+) |       `black`       |    `BLACK`    |     `0,0,0`     |        |                                          |
| ![#ff0000](https://placehold.it/20/ff0000/000000?text=+) |        `red`        |     `RED`     |    `255,0,0`    |  `R`   |                                          |
| ![#006e00](https://placehold.it/20/006e00/000000?text=+) |       `green`       |    `GREEN`    |    `0,110,0`    |  `G`   |                                          |
| ![#5c3317](https://placehold.it/20/5c3317/000000?text=+) |       `brown`       |    `BROWN`    |   `92,51,23`    |  `br`  |                                          |
| ![#0000c8](https://placehold.it/20/0000c8/000000?text=+) |       `blue`        |    `BLUE`     |    `0,0,200`    |  `B`   |                                          |
| ![#8b3a62](https://placehold.it/20/8b3a62/000000?text=+) | `magenta` or `pink` |   `MAGENTA`   |   `139,58,98`   |  `P`   |                                          |
| ![#0096b4](https://placehold.it/20/0096b4/000000?text=+) |       `cyan`        |    `CYAN`     |   `0,150,180`   |  `C`   |                                          |
| ![#969696](https://placehold.it/20/969696/000000?text=+) |    `light_gray`     |    `GRAY`     |  `150,150,150`  |  `lg`  | `light_`の代用として非推奨の`lt`を使用可 |
| ![#636363](https://placehold.it/20/636363/000000?text=+) |     `dark_gray`     |    `DGRAY`    |   `99,99,99`    |  `dg`  | `dark_`の代用として非推奨の`dk`を使用可  |
| ![#ff9696](https://placehold.it/20/ff9696/000000?text=+) |     `light_red`     |    `LRED`     |  `255,150,150`  |        | `light_`の代用として非推奨の`lt`を使用可 |
| ![#ff9696](https://placehold.it/20/ff9696/000000?text=+) |     `light_red`     |    `LRED`     |  `255,150,150`  |        | `light_`の代用として非推奨の`lt`を使用可 |
| ![#00ff00](https://placehold.it/20/00ff00/000000?text=+) |    `light_green`    |   `LGREEN`    |    `0,255,0`    |  `g`   | `light_`の代用として非推奨の`lt`を使用可 |
| ![#ffff00](https://placehold.it/20/ffff00/000000?text=+) |   `light_yellow`    |   `YELLOW`    |   `255,255,0`   |        | `light_`の代用として非推奨の`lt`を使用可 |
| ![#6464ff](https://placehold.it/20/6464ff/000000?text=+) |    `light_blue`     |    `LBLUE`    |  `100,100,255`  |  `b`   | `light_`の代用として非推奨の`lt`を使用可 |
| ![#fe00fe](https://placehold.it/20/fe00fe/000000?text=+) |   `light_magenta`   |  `LMAGENTA`   |   `254,0,254`   |  `lm`  | `light_`の代用として非推奨の`lt`を使用可 |
| ![#00f0ff](https://placehold.it/20/00f0ff/000000?text=+) |    `light_cyan`     |    `LCYAN`    |   `0,240,255`   |  `c`   | `light_`の代用として非推奨の`lt`を使用可 |
| ![#ffffff](https://placehold.it/20/ffffff/000000?text=+) |       `white`       |    `WHITE`    |  `255,255,255`  |  `W`   |                                          |

**注意:** 既定のRGB値は `\data\raw\colors.json`で定義されています。
**注意:** RGB値は`\config\base_colors.json`を編集することでカスタマイズ可能です。

## カラーの適用ルール

前景色と背景色の両方に影響を与える、特殊な色の変換処理が2種類あります:

- 反転;
- ハイライト.

**注意:** これらのカラー変換ルールは、テンプレートファイル (例:
`\data\raw\color_templates\no_bright_background.json`)で再定義可能です。

# Lua の型付き座標

ゲームでは、位置の算術演算を安全で自己説明的にするため、型付き座標システムを使用します。
すべての座標には **原点** と **スケール** という2つのメタデータがあります。互換性のない座標（例: オーバーマップ位置とマップタイル位置）を混ぜると、気づきにくい誤った結果ではなくランタイムエラーになります。

これは Lua でも同じです。**あらゆる場所で型付き座標を優先してください。生の `Tripoint` と
`Point` 型は純粋なオフセット演算にだけ使用します**。つまり、無次元の2Dまたは3Dベクトル（方向や固定変位など）を計算し、その結果を型付き座標にすぐ加算または減算する場合です。

---

## 原点

座標の **原点** は、その座標を測定した基準フレームを表します。

| 原点文字列 | 意味                                                                     |
| ---------- | ------------------------------------------------------------------------ |
| `"rel"`    | 無次元のオフセット。同じスケールのあらゆる座標に加算できます。           |
| `"abs"`    | 絶対的なゲーム世界の位置。全体で安定した唯一の原点です。                 |
| `"bub"`    | 現在のリアリティバブル（ロード済みマップ領域）の角を基準にした相対位置。 |
| `"mnt"`    | 回転を含むローカル車両（マウント）空間。`veh` スケールで使用します。     |
| `"sm"`     | 特定のサブマップの角を基準にした相対位置。                               |
| `"omt"`    | 特定のオーバーマップ地形タイルの角を基準にした相対位置。                 |
| `"mmr"`    | メモリーマップ領域の角を基準にした相対位置。                             |
| `"seg"`    | セグメントの角を基準にした相対位置。                                     |
| `"om"`     | 特定のオーバーマップの角を基準にした相対位置。                           |

`"rel"` 原点は特殊で、型付きの変位値として機能します。一致する2つの絶対座標を減算すると
`"rel"` の結果になり、`"rel"` 座標を非相対座標に加算すると、同じ非相対原点が返ります。

---

## スケール

座標の **スケール** は、各ステップの単位サイズを表します。

| スケール文字列      | 略称  | サイズ                                  |
| ------------------- | ----- | --------------------------------------- |
| `"map_square"`      | `ms`  | ゲームタイル1個                         |
| `"vehicle"`         | `veh` | 車両ローカルタイル                      |
| `"submap"`          | `sm`  | 12 × 12マップタイル                     |
| `"overmap_terrain"` | `omt` | サブマップ2個（24 × 24マップタイル）    |
| `"mem_map_region"`  | `mmr` | `MM_REG_SIZE` 個のサブマップ            |
| `"segment"`         | `seg` | `SEG_SIZE` 個のオーバーマップ地形タイル |
| `"overmap"`         | `om`  | `OMAPX` 個のオーバーマップ地形タイル    |

省略形（`ms`、`sm`、`omt` など）は、すべてのファクトリー関数名と、射影関数が受け取る文字列引数で使用されます。

---

## 型名

型付き座標の Lua 型名は、原点とスケールを PascalCase で組み合わせて作られます:

```
Tripoint<Origin><Scale>   →   TripointAbsMs, TripointBubSm, TripointRelOmt, …
Point<Origin><Scale>      →   PointAbsMs, PointRelSm, PointBubMs, …
```

有効な原点とスケールの組み合わせは31個あります。サポートされていない組み合わせ（例: `TripointAbsVeh`）を作ろうとするとランタイムエラーになります。

---

## 型付き座標の作成

### ファクトリー関数（関数型スタイル）

`coords` ライブラリーは、サポートされているすべての組み合わせについて
`coords.tripoint_<origin>_<scale>(x, y, z)` と `coords.point_<origin>_<scale>(x, y)` を提供します:

```lua
local player_pos  = gapi.get_avatar():get_pos_ms()   -- TripointBubMs from the API
local spawn_point = coords.tripoint_abs_ms(100, 200, 0)
local delta       = coords.tripoint_rel_ms(5, 0, 0)   -- typed offset
local omt_pos     = coords.tripoint_abs_omt(12, 8, 0)
```

原点とスケールを文字列で受け取る汎用コンストラクターもあります:

```lua
local p = coords.tripoint("abs", "ms", 100, 200, 0)
local q = coords.point("rel", "sm", 3, 3)
```

### 名前付きコンストラクター（OOP スタイル）

すべての型付き座標には、グローバルスコープに名前付きコンストラクターテーブルもあります:

```lua
local p = TripointAbsMs.new(100, 200, 0)      -- from x, y, z
local q = TripointAbsMs.new(raw_tripoint)      -- from raw Tripoint
local r = TripointAbsMs.new(point_coord, z)    -- from matching PointAbsMs + z
local s = TripointAbsMs.new()                  -- zero
```

どちらのスタイルでも同じオブジェクトが生成されます。一般にはファクトリースタイルの方が簡潔です。

---

## 座標成分の読み取り

```lua
local p = coords.tripoint_abs_ms(10, 20, 1)

print(p:x(), p:y(), p:z())   -- 10  20  1
print(p:origin())             -- "abs"
print(p:scale())              -- "ms"
print(p:type())               -- "TripointAbsMs"

local xy = p:xy()             -- PointAbsMs(10, 20); drops z, preserves origin/scale
local raw = p:raw()           -- raw Tripoint(10, 20, 1); strips all tags
```

成分は `set_x`、`set_y`、`set_z` で変更できます。

---

## 算術演算

### 加算

型付き座標には、次のものを加算できます:

- 生の `Point` または生の `Tripoint`。結果は元の原点とスケールを維持します。
- **同じスケール** の `"rel"` 型付き座標。同じ結果型になります。
- 別の `"rel"` 型付き座標に加えるオペランド。非相対の原点が優先されます。

```lua
local pos     = coords.tripoint_abs_ms(10, 20, 0)
local offset  = coords.tripoint_rel_ms(3, 0, 0)  -- typed relative offset

local new_pos = pos + offset          -- TripointAbsMs(13, 20, 0)
local also    = pos + Tripoint.new(0, 5, 0)  -- TripointAbsMs(10, 25, 0)  (raw as vector)
```

### 減算

`"rel"` 座標または生の値の減算は、加算の逆のように動作します。同じ原点とスケールを持つ一致した非相対座標を2つ減算すると、`"rel"` の結果になります:

```lua
local a   = coords.tripoint_abs_ms(15, 20, 0)
local b   = coords.tripoint_abs_ms(10, 20, 0)
local rel = a - b                              -- TripointRelMs(5, 0, 0)
```

### 乗算

整数スカラーを乗算できるのは `"rel"` 座標だけです:

```lua
local step  = coords.tripoint_rel_ms(1, 0, 0)
local five  = step * 5                          -- TripointRelMs(5, 0, 0)
```

### 等値と順序

2つの型付き座標は、原点、スケール、生の値がすべて一致するときだけ等しくなります。
`<` 演算子は `(origin, scale, raw)` を辞書順に並べるため、型付き座標をテーブルキーやソート済みコンテナで安全に使用できます。

---

## 射影

射影は、**原点を維持したまま** あるスケールから別のスケールへ座標を変換します。
ゲームのスケール階層は次のとおりです:

```
ms  <  sm  <  omt  <  mmr  <  seg  <  om
```

より粗いスケールへの射影では、負の無限大方向に丸められます（床除算）。

```lua
local abs_ms  = coords.tripoint_abs_ms(25, 26, 2)
local abs_omt = abs_ms:to_omt()   -- TripointAbsOmt(1, 1, 2)
local abs_sm  = abs_ms:to_sm()    -- TripointAbsSm(2, 2, 2)
```

すべての対象スケールには便利な短縮メソッドがあります: `:to_ms()`、`:to_sm()`、`:to_omt()`、
`:to_mmr()`、`:to_seg()`、`:to_om()`。汎用的な `:to(scale_string)` 形式も使用できます。

### project_remain: 商と余りに分割する

細かい座標がどの粗いタイルに含まれるかと、そのタイル内の位置の両方を知る必要がある場合は、
`project_remain` を使用します。2つの値を返します:

```lua
local abs_ms = coords.tripoint_abs_ms(25, 26, 2)
local quotient, remainder = abs_ms:project_remain_omt()
-- quotient  → TripointAbsOmt(1, 1, 2):   which overmap terrain tile
-- remainder → PointOmtMs(1, 2):          offset within that tile (fine scale, omt origin)
```

短縮メソッド: `:project_remain_sm()`、`:project_remain_omt()`、`:project_remain_mmr()`、
`:project_remain_seg()`、`:project_remain_om()`。汎用形式は `:project_remain("omt")` です。

`coords` ライブラリーには、自由関数としても次のものがあります:
`coords.project_remain(coord, scale_string)`、`coords.project_remain_omt(coord)` など。

### project_combine: 商と余りから再構成する

`project_combine` は `project_remain` の逆です。粗い座標と細かいオフセットを受け取り、細かいスケールの絶対座標を作ります:

```lua
local quotient, remainder = abs_ms:project_remain_omt()
local restored = coords.project_combine(quotient, remainder)
-- restored → TripointAbsMs(25, 26, 2)
```

インスタンスメソッド `quotient:project_combine(remainder)` としても使用できます。

---

## 距離関数

2つの型付き座標間の距離を計算するには、原点とスケールが同じである必要があります:

```lua
local a = coords.tripoint_abs_ms(10, 10, 0)
local b = coords.tripoint_abs_ms(13, 14, 0)

local rl  = coords.rl_dist(a, b)      -- rectilinear (Manhattan / Chebyshev) distance
local trig = coords.trig_dist(a, b)   -- Euclidean distance
local sq  = coords.square_dist(a, b)  -- square (Chebyshev) distance
```

インスタンスメソッドもあります: `a:rl_dist(b)`、`a:trig_dist(b)`、`a:square_dist(b)`。

---

## タイル列挙ヘルパー

`coords` ライブラリーには、標準タイル領域を覆う型付きポイント座標の配列を返すユーティリティがあります。領域の反復に便利です:

```lua
local sm_tiles  = coords.submap_tiles()           -- all PointSmMs in one submap
local bub_tiles = coords.tinymap_tiles()          -- all PointBubMs in the tinymap
local omt_tiles = coords.overmap_terrain_tiles()  -- all PointOmtMs in one overmap terrain tile
local om_tiles  = coords.overmap_tiles()          -- all PointOmMs in one overmap
```

---

## 生の Point と Tripoint を使う場合

`Point` と `Tripoint` はタグのない2D/3D整数ベクトルです。次の場合にだけ使用してください:

- 固有のゲーム世界上の意味を持たない純粋なオフセットを計算するとき（方向定数、隣接方向の変位、回転結果など）。
- 値を保存せず、型付き座標にすぐ加算または減算するとき。

```lua
-- Acceptable: raw Tripoint as a throwaway displacement vector
local neighbour = player_pos + Tripoint.new(1, 0, 0)

-- Preferred: named typed offset when the displacement has a scale context
local step = coords.tripoint_rel_ms(1, 0, 0)
local next  = player_pos + step
```

型付きの相当物がある場合、位置を生の `Tripoint` や `Point` として保存しないでください。
型付き形式なら演算時点でスケールの不一致によるバグを検出できますが、生の形式では誤ったマップ座標が気づかれずに生成されます。

---

## 有効な原点とスケールの組み合わせ

すべての原点がすべてのスケールで意味を持つわけではありません。サポートされている組み合わせは次のとおりです:

| 原点  | 有効なスケール                        |
| ----- | ------------------------------------- |
| `rel` | すべてのスケール                      |
| `abs` | `ms`、`sm`、`omt`、`mmr`、`seg`、`om` |
| `bub` | `ms`、`sm`                            |
| `mnt` | `veh` のみ                            |
| `sm`  | `ms` のみ                             |
| `omt` | `ms`、`sm`                            |
| `mmr` | `ms`、`sm`、`omt`                     |
| `seg` | `ms`、`sm`、`omt`、`mmr`              |
| `om`  | `ms`、`sm`、`omt`、`mmr`、`seg`       |

サポートされていない組み合わせを作成すると、無効な型名を示すメッセージとともにランタイムエラーになります。

---

## クイックリファレンス: coords ライブラリー API

| 関数                                             | 説明                                                         |
| ------------------------------------------------ | ------------------------------------------------------------ |
| `coords.tripoint(origin, scale, x, y, z)`        | 汎用型付き tripoint コンストラクター                         |
| `coords.point(origin, scale, x, y)`              | 汎用型付き point コンストラクター                            |
| `coords.tripoint_<o>_<s>(x, y, z)`               | 型付き tripoint ファクトリー（例: `coords.tripoint_abs_ms`） |
| `coords.point_<o>_<s>(x, y)`                     | 型付き point ファクトリー                                    |
| `coords.project_remain(coord, scale)`            | 座標を商と余りに分割                                         |
| `coords.project_remain_sm/omt/mmr/seg/om(coord)` | project_remain の短縮版                                      |
| `coords.project_combine(coarse, fine)`           | 分割した組から細かい座標を再構成                             |
| `coords.rl_dist(a, b)`                           | 直交（マンハッタン/チェビシェフ）距離                        |
| `coords.trig_dist(a, b)`                         | ユークリッド距離                                             |
| `coords.square_dist(a, b)`                       | 正方形（チェビシェフ）距離                                   |
| `coords.submap_tiles()`                          | 1つのサブマップ内にある全 `PointSmMs` オフセットの配列       |
| `coords.tinymap_tiles()`                         | ティニーマップ内にある全 `PointBubMs` オフセットの配列       |
| `coords.overmap_terrain_tiles()`                 | 1つの OMT 内にある全 `PointOmtMs` オフセットの配列           |
| `coords.overmap_tiles()`                         | 1つのオーバーマップ内にある全 `PointOmMs` オフセットの配列   |

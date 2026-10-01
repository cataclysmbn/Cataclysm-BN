# 車両

> [!NOTE]
>
> このページは現在作成中で、最近 `JSON INFO` から分割されました。

### 車両部品

車両に取り付ける車両コンポーネントです。

```json
"id": "wheel",                // 一意な識別子
"name": "wheel",              // 表示名
"symbol": "0",                // 部品が正常なときに表示する ASCII 文字
"looks_like": "small_wheel",  // タイルがない場合のタイルセットへのヒント。looks_like のタイルを使用
"color": "dark_gray",         // 部品が正常なときの色
"broken_symbol": "x",         // 部品が壊れたときに表示する ASCII 文字
"broken_color": "light_gray", // 部品が壊れたときの色
"damage_modifier": 50,         // （任意、デフォルト 100）部品が何かに当たったときのダメージ倍率（%）
"durability": 200,             // 壊れるまで部品が受けられるダメージ
"description": "A wheel."     // 取り付け時に表示する部品の説明
"size": 2000                   // FLUIDTANK フラグなら mL 容量、それ以外は 4 で割った空間の体積
"wheel_width": 9,              /* （任意、デフォルト 0）
                                * 特殊: 部品は次のフィールドを最大1つだけ持てます:
                                * wheel_width = インチ単位の基本ホイール幅
                                * size        = トランク/箱の収納体積
                                * power       = ワット単位の基本エンジン出力
                                * bonus       = 付与されるボーナス。マフラーは騒音低減%、シートベルトは車両から投げ出されないボーナス
                                * par1        = ヘッドライトの光量など固有ボーナス用の汎用値 */
"wheel_type":                 // （任意: standard, off-road）
"contact_area":               // （任意）車両の接地圧に影響
"cargo_weight_modifier": 33,  // （任意、デフォルト 100）指定した割合で貨物重量を変更
"weight_modifier": 33,        // （任意、デフォルト 100）指定した割合で部品の基本重量を変更
"fuel_type": "NULL",          // （任意、デフォルト "NULL"）部品が消費する燃料/弾薬の item id

"item": "wheel",              // 部品の取り付けに使い、取り外すと得られるアイテム
"difficulty": 4,              // 取り付けに必要な力学スキルの最低レベル
"breaks_into" : [             // 部品が破壊されたとき、アイテムグループのアイテムを周囲の地面に生成
  {"item": "scrap", "count": [0,5]} // 配列の代わりにインラインアイテムグループも可能
],
"breaks_into" : "some_item_group", // アイテムグループの ID だけでも可能
	"flags": [                    // 部品に関連するフラグ
	     "EXTERNAL", "MOUNT_OVER", "WHEEL", "MOUNT_POINT", "VARIABLE_SIZE"
	],
	"rotating_light": {           // 任意。通常の light フラグの形ではなく回転する指向性円錐光を出すライト部品用
	  "arc": 30,                  // 円錐の幅（度）。デフォルト 30
	  "step": 90,                 // 周期ごとに進む角度。負数なら逆回転。デフォルト 90
	  "phase": 0,                 // 初期方向オフセット（度）。デフォルト 0
	  "period": "1 turns",        // 回転ステップの間隔。整数のターン数も可能。デフォルト 1 ターン
	  "beams": 2                  // 同時に表示する均等間隔の光線数。デフォルト 2
	},
	"damage_reduction" : {        // 下記のような固定ダメージ減少。未指定なら 0
	    "all" : 10,
	    "physical" : 5
},
                              // 以下の任意フィールドは ENGINE 部品専用
"m2c": 50,                    // ENGINE フラグには必須。巡航出力と最大出力の比率
"backfire_threshold": 0.5,    // 任意、デフォルト 0。逆火を発生させる損傷 HP/最大 HP の最大比率
"backfire_freq": 20,          // 閾値が 0 より大きい場合は必須。逆火の X 分の 1 の確率
"noise_factor": 15,           // 任意、デフォルト 0。エンジン出力に掛けて騒音を決める
"damaged_power_factor": 0.5,  // 任意、デフォルト 0。損傷時の出力倍率計算に使用
"muscle_power_factor": 0,     // 任意、デフォルト 0。筋力8を超える1ポイントごとに出力を加算し、下回ると減算
"exclusions": [ "souls" ]     // 任意、デフォルト空。除外語を共有するエンジンは同じ車両に取り付け不可
"fuel_options": [ "soul", "black_soul" ] // 任意、デフォルトは fuel_type。利用可能な燃料タイプのリスト
"comfort": 3,                 // 任意、デフォルト 0。地形/家具の快適さ。そこで眠れるかに影響
"floor_bedding_warmth": 300,  // 任意、デフォルト 0。睡眠時に提供する追加の暖かさ
"bonus_fire_warmth_feet": 200,// 任意、デフォルト 300。近くの火から足に受ける暖かさを増加
"height": 5,                  // 任意、気球の高さ（m、揚力の倍率）
"lift_coff": 0.5,             // 任意、翼の効果倍率
"propeller_diameter": 0.5,   // 任意、プロペラの直径
"length": 3,                  // 任意、はしごの z レベル長
"default_color": "#622625"   // 任意、他の色がない場合のデフォルト色調
```

### 統合ツール

```json
"integrated_tools": [ "foo" ],
```

これは車両部品がクラフト用に提供するツールの配列です。家具の `crafting_pseudo_item` と比較できます。動作させるには車両部品に `CRAFTING` フラグが必要です。

古いクラフト用車両部品フラグの大半は削除され、同等のツールに置き換える必要があります。調べると特定機能を提供する `WATER_PURIFIER`、`FAUCET`、`WATER_FAUCET` フラグは残されています。

```json
"integrated_tools": [ "pot", "pan", "hotplate" ],  // `KITCHEN` フラグを置き換える
"integrated_tools": [ "dehydrator", "vac_sealer", "food_processor", "press" ],  // `CRAFTRIG` を置き換える
"integrated_tools": [ "chemistry_set", "electrolysis_kit" ],  // `CHEMLAB` を置き換える
"integrated_tools": [ "forge" ],  // `FORGE` を置き換える
"integrated_tools": [ "fake_adv_butchery" ],  // `BUTCHER_EQ` フラグと併用
"integrated_tools": [ "kiln" ],  // `KILN` を置き換える
"integrated_tools": [ "soldering_iron", "welder" ],  // ツールを置き換える
"integrated_tools": [ "water_purifier" ],  // ツールを置き換えるが、WATER_PURIFIER の車両タンク浄水能力は置き換えない
```

### 変換器

`CONVERTER` フラグを持つ車両が燃料タンク内で液体を別の液体へ変換できるようにするフィールドです。
アイテムや近くの液体供給源には使用できません。

```json
"converter": {
  "input": "input_itype_id",   // 入力する itype
  "input_step": 3,              // 各反復で消費する入力量
  "output": "output_itype_id", // 出力する itype
  "output_step": 1,             // 各反復で消費する出力量
  "max_steps": 10,              // 1分あたりの最大反復数
  "charge_cost": 100            // 各反復に必要な kJ（車両での最小単位）
}
```

### 部品の耐性

```json
"all" : 0.0f,        // すべての耐性の初期値。より具体的なタイプで上書き
"physical" : 10,     // bash、cut、stab の初期値
"non_physical" : 10, // acid、heat、cold、electricity、biological の初期値
"biological" : 0.2f, // 特定タイプの耐性。一般値を上書き
"bash" : 3,
"cut" : 3,
"acid" : 3,
"stab" : 3,
"heat" : 3,
"cold" : 3,
"electric" : 3
```

### 形状

異なるスプライトを持つ車両部品の copy-from 版を自動的に作成するフィールドです。
抽象定義から適切なオブジェクトを自動作成します。

注: このフィールドは copy-from をサポートしないため、各オブジェクトに定義する必要があります。

```jsonc
"shapes": [
  {
    "direction": "left",     // ID に追加される値。wing_metal は wing_metal_left になり、looks_like にも追加
    "symbol": "y",           // 上記の symbol 定義と同じ
    "broken_symbol": "y",    // 上記の broken_symbol 定義と同じ
    "looks_like": "board_nw" // 上記の looks_like 定義と同じ。direction の変更を上書き
  },
]
```

### 車両

`vehicles_spawning.md` も参照してください。

```json
"id": "shopping_cart",                     // 内部で使用する名前
"name": "Shopping Cart",                   // 表示名。i18n の対象
"blueprint": "#",                          // 車両のプレビュー。コードでは無視されるため説明としてのみ使用
"parts": [                                 // 部品リスト
    {"x": 0, "y": 0, "part": "box"},       // 部品定義。正の x 方向は左、正の y 方向は右
    {"x": 0, "y": 0, "part": "casters"}    // 部品 ID は vehicle_parts.json を参照
]
                                           /* 重要: 車両部品はゲームで取り付ける順序（フレームとマウントポイントが先）で定義する必要があります。
                                            * 通常の取り付けルールにも従う必要があります
                                            * （重ねられない部品フラグを重ねることはできません）。 */
```

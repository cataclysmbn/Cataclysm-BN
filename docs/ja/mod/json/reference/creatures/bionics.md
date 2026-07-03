# バイオニック

> [!WARNING]
> 多くのバイオニックの効果はコード内でのみ定義されており、ここにはリストされていません

新しいバイオニックを追加するときに、それが別のバイオニックに含まれていない場合は、対応する CBM も追加する必要があります。
[ここ](#paired-item-definition) で説明されている項目。たとえ故障したバイオニックであっても。

### フィールド

| 識別子                     | 説明                                                                                                                                            |
| -------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| id                         | (_必須_) 一意の文字列 ID                                                                                                                        |
| name                       | (_必須_) バイオニックのゲーム名                                                                                                                 |
| description                | (_必須_) ゲーム内のバイオニックの説明                                                                                                           |
| flags                      | (_任意_) json フラグの配列。 [リスト](/mod/json/reference/json_flags/#bionics)を参照してください。                                              |
| capacity                   | (_任意_) このバイオニックから得られるバイオニックパワー                                                                                         |
| act_cost                   | (_オプション_) バイオニックを起動するための電力コスト。                                                                                         |
| deact_cost                 | (_オプション_) バイオニックを無効にするための電力コスト。                                                                                       |
| trigger_cost               | (_任意_) バイオニック効果を発動する際の電力コスト。                                                                                             |
| kcal_trigger_cost          | (_オプション_) バイオニック効果を発動する際のカロリー消費                                                                                       |
| react_cost                 | (_任意_) `time` ティックごとに消費する電力量                                                                                                    |
| time                       | (_任意_) `react_cost` 電力を消費する間隔                                                                                                        |
| stat_bonus                 | (_任意_) 配列の配列、内部配列は `["STAT", integer bonus]`                                                                                       |
| fuel_options               | (_オプション_) このバイオニックを強化するために受け入れられる Itype ID                                                                          |
| fuel_capacity              | (_オプション_) バイオニックによって保管される燃料のチャージ                                                                                     |
| fuel_efficiency            | (_オプション_) 燃料の消費によって生成される電力の乗数                                                                                           |
| fuel_multiplier            | (_オプション_) 充電ごとにバイオニックにロードされる燃料の量を倍増します。                                                                       |
| passive_fuel_efficiency    | (_任意_) `fuel_efficiency` ですが、バイオニックは非アクティブです。`PERPETUAL` 燃料にも使用されます                                             |
| coverage_power_gen_penalty | (_任意_) `fuel_efficiency` に `occupied_bodyparts` のアイテムのカバレッジとこの値を乗算します (50% ボディパーツ カバレッジ +)                   |
| exothermic_power_gen       | (_オプション_) 動力に変換されない燃料が熱に変換される場合                                                                                       |
| remote_fuel_draw           | (_オプション_) 接続された外部ケーブルからの電力供給 (I.E. ケーブル充電器システム)                                                               |
| power_gen_emission         | (_任意_) [フィールド ID](/mod/json/reference/map/field_type) 電力を生成するときに生成します                                                     |
| weight_capacity_modifier   | (_オプション_) 耐荷重の乗数                                                                                                                     |
| weight_capacity_bonus      | (_オプション_) 耐荷重の追加                                                                                                                     |
| upgraded_bionic            | (_オプション_) このバイオニックをインストールすることでアップグレードできるバイオニック ID                                                      |
| required_bionics           | (_オプション_) インストールに必要なバイオニック ID の配列                                                                                       |
| included_bionics           | (_オプション_) インストール時に指定されるバイオニック ID の配列                                                                                 |
| included                   | (_任意_) ブール値、このバイオニックは別のバイオニックに含まれていますか                                                                         |
| available_upgrades         | (_オプション_) アップグレードできるバイオニック ID の配列                                                                                       |
| env_protec                 | (_任意_) 配列の配列、内部配列は `["bodypart", integer]` で、整数はボディ部分の環境保護を表します。                                              |
| bash_protec                | (_任意_) 配列の配列、内部配列は `["bodypart", integer]` で、整数はボディ部分の bash 保護です。                                                  |
| cut_protec                 | (_任意_) 配列の配列、内部配列は `["bodypart", integer]` で、整数はボディ部分のカット保護です                                                    |
| bullet_protec              | (_任意_) 配列の配列、内部配列は `["bodypart", integer]` で、整数はボディ部分の弾道保護を表します。                                              |
| occupied_bodyparts         | (_任意_) 配列の配列、内部配列は `["bodypart", integer]` で、整数はボディパーツで占有されているバイオニック スロットです。                       |
| encumbrance                | (_任意_) 配列の配列、内部配列は `["bodypart", integer]` で、整数はボディ部分の負担になります。                                                  |
| canceled_mutations         | (_任意_) インストール時にキャンセルされ、その後は防止される特性 ID の配列                                                                       |
| learned_spells             | (_任意_) 配列の配列、内部配列は `["spell", integer]` で、整数は呪文を学習するレベルです。                                                       |
| enchantments               | (_オプション_) バイオニックに与えるエンチャント ID の配列                                                                                       |
| bio_enchantments           | (_任意_) バイオニックで与えるインライン エンチャントの配列                                                                                      |
| fake_item                  | (_任意_) バイオニックが与えるアイテム。 `BIONIC_TOOLS` では道具として与えられ、`BIONIC_TOGGLED` では振り回され、`BIONIC_GUN` では発射されます。 |
| can_uninstall              | (_オプション_) バイオニックをアンインストールできる場合                                                                                         |
| no_uninstall_reason        | (_任意_) `can_uninstall` では必須、アンインストールできない旨のメッセージが表示される                                                           |
| starting_bionic            | (_任意_) true の場合、バイオニックから開始できます                                                                                              |
| points                     | (_任意_) キャラクター作成時に必要なポイント数                                                                                                   |

```jsonc
{
  "id": "bio_chaos",
  "name": "Complete Chaos",
  "description": "This does a lot of things that makes little sense",
  "flags": ["BIONIC_TOGGLED", "BIONIC_NPC_USABLE", "COMBAT_NPC_USE"],
  "capacity": "100 kJ",
  "act_cost": "5 J",
  "deact_cost": "5 J",
  "trigger_cost": "5 J",
  "react_cost": "5 J",
  "kcal_trigger_cost": 5,
  "time": 1,
  "stat_bonus": [["STR", 2]],
  "fuel_options": ["gasoline"],
  "fuel_capacity": 500,
  "fuel_efficiency": 0.25,
  "fuel_multiplier": 2,
  "coverage_power_gen_penalty": 0.5,
  "passive_fuel_efficiency": 0.125,
  "exothermic_power_gen": true,
  "remote_fuel_draw": "1 kJ",
  "powergen_emission": "fd_fire",
  "weight_capacity_modifier": 2.0,
  "weight_capacity_bonus": "20 kg",
  "upgraded_bionic": "bio_chaos_v1",
  "required_bionics": ["bio_chaos_base"],
  "included_bionics": ["bio_chaos_stuff", "bio_chaos_morestuff"],
  "included": false,
  "available_upgrades": ["bio_chaos_v50"],
  "env_protec": [["eyes", 7]],
  "bash_protec": [["eyes", 3]],
  "cut_protec": [["eyes", 3]],
  "bullet_protec": [["eyes", 3]],
  "occupied_bodyparts": [["torso", 1]],
  "encumbrance": [["mouth", 10]],
  "canceled_mutations": ["ASTHMA"],
  "learned_spells": [["mint_breath", 2]],
  "enchantments": ["ENCH_UNCANNY_DODGE"],
  "bio_enchantments": [
    { "condition": "ALWAYS", "values": [{ "value": "SPEED", "multiply": 0.1 }] },
  ],
  "can_uninstall": false,
  "no_uninstall_reason": "The telescopic lenses are part of the patient's eyes now, remove them and the patient would become blind",
  "starting_bionic": true,
  "points": 1,
}
```

<a id="paired-item-definition"></a>

### ペア項目の定義

```jsonc
{
  "id": "bio_adrenaline", // バイオニック ID と正確に一致する必要があります
  "copy-from": "bionic_general", // これにはいくつかのバリエーションがありますが、これが基本的なものです
  "type": "BIONIC_ITEM", // 必須のタイプ
  // 残りは一般項目欄です
  "name": { "str": "Adrenaline Pump CBM" },
  "looks_like": "bio_int_enhancer",
  "description": "A stimulator system that is implanted alongside the adrenal glands, allowing the user to trigger their body's adrenaline response at the cost of some bionic power.",
  "price": "40 USD",
  "weight": "250 g",
  "difficulty": 6
},
```

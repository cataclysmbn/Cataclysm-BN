# 呪文、エンチャント、およびその他のカスタム効果

# 呪文

`data/json/debug_spells.json` にはテンプレート呪文があり、参考のためここにコピーしてあります:

```json
{
  // この呪文は JSON 内でテンプレートとして存在しており、貢献者が使用可能な値を確認できるようになっています
  "id": "example_template", // 呪文のID。内部で使用されます。翻訳不要
  "type": "SPELL",
  "name": "Template Spell", // ゲーム内に表示される呪文名
  "description": "This is a template to show off all the available values",
  "blocker_mutations": ["THRESH_RAT"], // この呪文を唱えることを妨げる変異のリスト
  "valid_targets": ["hostile", "ground", "self", "ally"], // 有効ターゲットに含まれていない対象には呪文を唱えられません。
  "effect": "shallow_pit", // 効果は C++ で実装されています。下記に利用可能な効果のリストがあります。
  "effect_str": "template", // 特別なフィールド。詳細は下記参照のこと
  "extra_effects": [{ "id": "fireball", "hit_self": false, "max_level": 3 }], // 1つの呪文で複数の効果を発動できます。
  "melee_dam": ["cut", "elemental"], // 指定されたタイプの近接ダメージが呪文ダメージに加算されます。標準的なダメージタイプ文字列と、利便性のため 'physical' と 'elemental' のカテゴリもサポートしています。
  "affected_body_parts": [
    "HEAD",
    "TORSO",
    "MOUTH",
    "EYES",
    "ARM_L",
    "ARM_R",
    "HAND_R",
    "HAND_L",
    "LEG_L",
    "FOOT_L",
    "FOOT_R"
  ], // 効果が及ぶ体の部位
  "spell_class": "NONE", //
  "scale_str": true, // 呪文の威力が筋力に依存してスケーリングされます。他に scale_dex, scale_per, scale_int も使用可能です。
  "base_casting_time": 100, // 呪文の詠唱時間(ムーブ単位)
  "base_energy_cost": 10, // 呪文を詠唱するために必要なエネルギー量(指定された種類)
  "energy_source": "MANA", // 呪文を詠唱するために使用するエネルギーの種類。MANA, BIONIC, HP, STAMINA, FATIGUE, NONE (NONE はマナを消費しない)
  "components": [requirement_id],                            // 呪文の詠唱に必要な構成要素。クラフトで使用するものと同じ形式のID
  "difficulty": 12, // 呪文の習得/詠唱難易度
  "max_level": 10, // 呪文の最大レベル
  "min_damage": 0, // 最低ダメージ(または「初期」ダメージ)
  "max_damage": 100, // 呪文が到達可能な最大ダメージ
  "damage_increment": 2.5, // ダメージ(および下記のステータス)を計算する際に、呪文レベルに掛けて最小ダメージに加算する値
  "min_aoe": 0, // 効果範囲（現在未実装）
  "max_aoe": 5,
  "aoe_increment": 0.1,
  "min_range": 1, // 呪文の射程
  "max_range": 10,
  "range_increment": 2,
  "min_accuracy": 50, // 呪文の「命中率」(％)。どの体の部位にヒットしたかを判定する際に使用
  "max_accuracy": 70,
  "accuracy_increment": 2,
  "min_dot": 0, // 継続ダメージ(Damage over Time)。現在未実装
  "max_dot": 2,
  "dot_increment": 0.1,
  "min_duration": 0, // 呪文効果の持続時間(特別な効果がある場合)
  "max_duration": 1000,
  "duration_increment": 4,
  "min_pierce": 0, // 呪文が防具を貫通する割合(現在未実装)
  "max_pierce": 1,
  "pierce_increment": 0.1,
  "field_id": "fd_blood", // 生成するフィールドの文字列 ID(現在は固定)
  "field_chance": 100, // AoE 内の各タイルでフィールドが生成される確率(1/field_chance)
  "min_field_intensity": 10, // 生成されるフィールドの強度(最小)
  "max_field_intensity": 10, // 生成されるフィールドの強度(最大)
  "field_intensity_increment": 1,
  "field_intensity_variance": 0.1, // tフィールド強度のばらつき。例：この値なら強度は 9～11 の範囲になります。
  "sound_type": "combat", // 音の種類。可能なタイプ：background, weather, music, movement, speech, activity, destructive_activity, alarm, combat, alert, order
  "sound_description": "a whoosh", // 音の説明。既定値は "an explosion"。通常は "You hear %s" の形式で表示します。
  "sound_ambient": true, // 環境音として扱うかどうか
  "sound_id": "misc", // 音のID
  "sound_variant": "shockwave", // 音のバリエーション
  "sprite": "fd_electricity", // 呪文のスプライトを任意の有効なフィールド ID に変更します。NO_EXPLOSION_VFX とは併用しないこと
  "learn_spells": { "acid_resistance_greater": 15 } // この呪文レベルが指定値以上になると、指定された呪文を習得します。
}
```

上記のほとんどの既定値は 0 または "NONE" であるため、呪文に関係ない項目は省略して構いません。

一部の値を決める際には、計算式が線形ではないことに注意する必要があります。
例えば、呪文失敗率の計算式は以下の通りです:

`( ( ( ( spell_level - spell_difficulty ) * 2 + intelligence + spellcraft_skill ) - 30 ) / 30 ) ^ 2`

つまり、難易度 0 の呪文を、知能 8、魔法技能 0、呪文レベル 0 のプレイヤーが詠唱すると、53% の確率で失敗 します。
一方、知性 12、魔法技能 6、呪文レベル 6 のプレイヤーなら、失敗率は 0% になります。

ただし、経験値の獲得量の計算はもう少し複雑です。
レベルアップに必要な経験値の計算式は以下の通りです:

`e ^ ( ( level + 62.5 ) * 0.146661 ) ) - 6200`

#### 現在実装されている呪文フラグ

- `PERMANENT` - この呪文で生成されたアイテムやクリーチャーは通常通り消えたり死んだりしません。

- `IGNORE_WALLS` - 呪文の AoE が壁を貫通します。

- `SWAP_POS` - 遠距離呪文で使用すると、詠唱者と対象位置の入れ替えを行います。（途中にいるクリーチャーとも位置を交換）

- `HOSTILE_SUMMON` - 召喚呪文で常に敵対的なモンスターを召喚します。

- `HOSTILE_50` - 召喚モンスターは 50% の確率で味方として出現します。

- `SILENT` - 対象位置で音を出しません。

- `NO_EXPLOSION_VFX` - 呪文に爆発エフェクトを表示しません。

- `LOUD` - 対象位置で追加の音を出します。

- `VERBAL` - 詠唱者の位置で音が鳴ります。口の負荷が失敗率に影響します。

- `SOMATIC` - 腕の負荷が失敗率と詠唱時間にわずかに影響します。

- `NO_HANDS` - 手の負荷が呪文のエネルギー消費に影響しません。

- `UNSAFE_TELEPORT` - テレポート呪文で詠唱者や他者が死亡するリスクがあります。

- `NO_LEGS` - 足の負荷が詠唱時間に影響しません。

- `CONCENTRATE` - 集中度が呪文の失敗率に影響します。

- `RANDOM_AOE` - 通常の動作ではなく、min + increment\* level と max の間でランダムな AoE を選びます。

- `RANDOM_DAMAGE` - 通常の動作ではなく、min + increment\* level と max の間でランダムなダメージを選びます。

- `DIVIDE_DAMAGE` - 呪文のダメージを、命中したすべてのクリーチャーに均等に分配する

- `RANDOM_DURATION` - 通常の動作ではなく、min + increment\* level と max の間でランダムな持続時間を選びます。

- `RANDOM_TARGET` - 通常の動作ではなく、有効射程内のランダムな対象を選びます。

- `MUTATE_TRAIT` - 呪文効果を上書きして、カテゴリーではなく特定の trait_id を使用します。

- `WONDER` - extra_spellsのすべてを詠唱する代わりに、N 個を選んで詠唱します。 (N は std::min(damage(), number_of_spells))

- `PAIN_NORESIST` - 痛み系呪文が抵抗されません。(deadened trait のように)

- `BRAWL` - Brawler trait を持つキャラクターが詠唱可能です。 (持たない場合は不可)

- `DUPE_SOUND` - 同じ音を複数回再生可能です。(対象ごとに音を鳴らすなど)

- `ADD_MELEE_DAM` - 現在装備している武器の最高カテゴリ近接ダメージを呪文ダメージに加算します。(LEGACY。Bash/Stab/Cut のみ対応。代わりに melee_dam フィールドを使用推奨)

- `PHYSICAL` - BRAWL を暗黙的に含み、Brawler にボーナスを与えます。主に「物理技術」用

- `MOD_MELEE_MOVES` - 武器の近接攻撃ムーブコストを呪文の詠唱コストに加算。特殊動作：
  base_casting_time が負の場合、increment は行動コストの倍率として扱います。(呪文版の 連撃(Rapid Strike)再現に便利)

- `MOD_MEELE_STAM` - `MOD_MELEE_MOVES` と同様ですが、武器のスタミナ消費に対応。スタミナ技術向
  け。負の base_cost と increment フィールドに関する特殊動作も同様です。

- `NO_FAIL` - この呪文は詠唱時に失敗しません。

#### Currently Implemented Effects and special rules

- `pain_split` - 自分のすべての手足のダメージを均等化します。

- `move_earth` - 対象地点の地面を「掘る」。一部の地形は掘れません。

- `target_attack` - 対象にダメージを与えます(壁は無視)。負のダメージは回復になります。
  effect_str が指定されていれば、その効果(JSON内で定義されたもの)を対象の affected_body_parts に適用します。AoE がある場合は対象を中心とした円形範囲で発動し、有効なターゲットにのみダメージを与えます。（AoE は壁を無視しない）

- `projectile_attack` - target_attackと似ていますが、発射した弾は通れない地形で停止します。effect_str が指定されていれば対象の affected_body_parts に効果を適用します。

- `cone_attack` - 対象に向かって円錐状の範囲で攻撃。範囲の角度は aoe。壁で停止。effect_str が指定されていれば対象の affected_body_parts に効果を適用します。

- `line_attack` - 対象に向かって幅 aoe の直線攻撃。途中で壁に阻まれます。
  effect_str が指定されていれば対象の affected_body_parts に効果を適用します。

- `spawn_item` - 指定したアイテムを生成。持続時間終了後に消えます。既定の持続時間は 0。ダメージ
  で生成数を決定します。

- `summon_vehicle` - 指定した乗り物を生成。持続時間終了後に消えます。

- `summon` - モンスターを生成。持続時間終了後に消えます。既定ではプレイヤーに友好的です。ダメー
  ジで生成数を決定します。

- `translocate` - プレイヤーを登録済みの「転送装置」間で移動します。（例：Magical Nights のゲー
  ト）

- `teleport_random` - プレイヤーをランダムに範囲内へテレポート（AoE 変動あり）

- `recover_energy` - 呪文のダメージに相当する量のエネルギー源（effect_str で指定）を回復します。

* "MANA"
* "STAMINA"
* "FATIGUE"
* "PAIN"
* "BIONIC"

- `ter_transform` - 対象を中心とした範囲内の地形や家具を変化させます。範囲内の各ポイントが変化
  する確率は one_in(damage)。effect_str は ter_furn_transform の ID

- `vomit` - 範囲内のクリーチャーは可能であれば即座に嘔吐します。

- `timed_event` - プレイヤーにのみタイムドイベントを追加。使用可能なイベント: "help", "wanted",
  "robot_attack", "spawn_wyrms", "amigara", "roots_die", "temple_open", "temple_flood",
  "temple_spawn", "dim", "artifact_light" ※アーティファクトのアクティブ効果用に追加されたもので、サポートは限定的。使用は自己責任

- `explosion` - 対象を中心に爆発を発生。威力は damage()、範囲は aoe()/10

- `flashbang` - 対象を中心にフラッシュバン効果を発生。威力は damage()、範囲は aoe()/10

- `mod_moves` - 対象の行動ポイントに damage() を加算。負の値で対象を一定時間「凍結」可能です。

- `map` - プレイヤーを中心にオーバーマップを範囲 aoe() までマッピングします。

- `morale` - 範囲内の NPC またはアバターに damage() の士気効果を付与します。減衰開始は duration()/10です。

- `charm_monster` - HP が damage() 未満のモンスターを約 duration() ターン魅了します。

- `mutate` - 対象を突然変異させます。effect_str が指定されていれば、そのカテゴリー方向に変異。
  MUTATE_TRAIT フラグで特定の trait を指定可能です。成功確率は damage()/100（10000 = 100%）です。

- `bash` - 対象の地形を破壊します。破壊力は damage()です。

- `dash` - プレイヤーを対象タイルまで移動します。フィールドを残すことも可能です。

- `area_push` - 範囲内の対象を一点から外向きに押します。

- `directed_push` 単方向に対象を押します。

- `noise` 呪文の damage() に応じた音量で音を発生します。

- `WONDER` - 上記のものとは異なり、これは「効果」ではなく「フラグ」です。これにより親呪文の動作が大
  きく変更されます：呪文自体は発動しませんが、そのダメージと範囲の情報が使用されて、追加効果(extra_effects)が発動します。追加効果のうち、N個がランダムに選ばれて発動し、ここでのNは呪文の現在のダメージ(RANDOM_DAMAGEフラグとスタックします)に基づきます。また、この呪文が発動した際のメッセージも表示されます。もし呪文のメッセージを表示したくない場合は、メッセージを空文字列に設定してください。

- `RANDOM_TARGET` - これは、呪文が対象をキャスターではなくランダムに選択するよう強制する特別な呪文
  フラグです (wonderフラグと同様)。これも追加効果(extra_effects) に影響します。

##### 攻撃タイプを持つ呪文のダメージタイプ (ケースインセンシティブ):

- `fire` - 火
- `acid` - 酸
- `bash` - 打撃
- `bullet` - 弾
- `bio` - 生物的ダメージ、例えば毒など
- `cold` - 冷気
- `cut` - 斬撃
- `electric` - 電気
- `light` - 光、実際の光と「聖なる光」にも使用
- `dark` - 闇
- `psi` - 精神的ダメージ
- `stab` - 刺突
- `true` - このダメージタイプは防具を完全に無視するため非常に強力です。未指定の場合、既定のダメージタイプとなります。

#### レベルアップする呪文

レベルアップに伴って効果が変化する呪文には、最小値と最大値、および増分が必要です。最小値は呪文がレベル0で発動する効果、最大値は呪文の成長が止まる効果で、増分はレベルごとにどれだけ変化するかを示します。例:

```json
"min_range": 1,
"max_range": 25,
"range_increment": 5,
```

最小値と最大値は常に同じ符号でなければならないことに注意してください。ただし、負の「回復」効果を持ち、痛みやスタミナダメージを与える呪文のように、負の値を使うことも可能です。例えば:

```json
{
  "id": "stamina_damage",
  "type": "SPELL",
  "name": "Tired",
  "description": "decreases stamina",
  "valid_targets": ["hostile"],
  "min_damage": -2000,
  "max_damage": -10000,
  "damage_increment": -3000,
  "max_level": 10,
  "effect": "recover_energy",
  "effect_str": "STAMINA"
}
```

### 呪文の習得方法

呪文を習得する方法には3つの方法があります:

1. 変異によって呪文を習得。`spells_learned`フィールドを使って、習得した呪文とそのレベルを指定できます。
2. 特定の呪文は、適切なレベルに達することで他の呪文を教えてくれることがあります(learn_spells変数を使用)。
3. アイテムを使用することによって呪文を学ぶ。これが呪文を訓練する唯一の方法です(使う以外では訓練できません)。

以下の例は、3つの方法をすべて示しています:

```json
{
  "id": "DEBUG_spellbook",
  "type": "GENERIC",
  "name": "A Technomancer's Guide to Debugging C:DDA",
  "description": "static std::string description( spell sp ) const;",
  "weight": 1,
  "volume": "1 ml",
  "symbol": "?",
  "color": "magenta",
  "use_action": {
    "type": "learn_spell",
    "spells": [ "debug_hp", "debug_stamina", "example_template", "debug_bionic", "pain_split", "fireball" ] // このアイテムで学べる呪文のリスト
  }
},
```

以下の例では、酸耐性がレベル15に達することで「Greater Acid Resistance」という呪文を学びます:

```json
{
    "id": "acid_resistance",
    "type": "SPELL",
    "name": { "str": "Acid Resistance" },
    "description": "Protects the user from acid.",
    ...
    "learn_spells": { "acid_resistance_greater": 15 }
}
```

この呪文書を学ぶには、知性、呪文能力、集中力に応じて、1ターンあたり約1経験値を得ることができます。

```json
"spells_learned": [ [ "debug_hp", 1 ], [ "debug_stamina", 1 ], [ "example_template", 1 ], [ "pain_split", 1 ] ],
```

#### 職業やNPCクラスでの呪文

職業やNPCクラスに「呪文」フィールドを追加することができます。例えば:

```json
"spells": [ { "id": "summon_zombie", "level": 0 }, { "id": "magic_missile", "level": 10 } ]
```

注釈: この方法を使うと、本来クラス(職業・魔法クラスなど)を習得させるための呪文を覚えることが可能になってしまいます。また、そのクラスを獲得するためのプロンプト(確認メッセージ)は表示されません。
職業定義にこの項目を追加する際は慎重に扱ってください。

#### モンスターの呪文

モンスターに特殊攻撃として呪文を設定することができます。

```json
{ "type": "spell", "spell_id": "burning_hands", "spell_level": 10, "cooldown": 10 }
```

- spell_id: 発動する呪文のID
- sspell_level: 呪文が発動するレベル。モンスターが使う呪文はプレイヤーの呪文のようにレベルアップしません。
- cooldown: モンスターがこの呪文を発動するまでのクールダウン時間

# エンチャント

エンチャントは、アイテム、バイオニック、または変異によるカスタム効果を指定するために使用されます。

## フィールド

### id

(文字列) このエンチャントの固有の識別子。

### has

(文字列) エンチャントが有効になるために必要な条件を指定します。

主にアイテムの場合

値:

- `HELD` (既定値) - 所持品内にあるとき
- `WIELD` - 手に持ったとき
- `WORN` - 防具として着用したとき

### condition

(文字列) エンチャントが有効になるために、使用者が適切な環境にいるかどうかを判定する方法を指定します。

値:

- `ALWAYS` (既定値) - 常に有効
- `UNDERGROUND` - アイテムの所持者が Zレベル 0 より下にいる場合
- `ABOVEGROUND` - アイテムの所持者が Zレベル 0 以上にいる場合
- `UNDERWATER` - 所持者が泳げる地形(水中)にいる場合
- `NIGHT` - 夜間である場合
- `DUSK` - 夕暮れである場合
- `DAY` - 昼間である場合
- `DAWN` - 夜明けである場合
- `ACTIVE` - エンチャントが付与されているアイテム・変異・バイオニックなどが 有効状態 のとき
- `INACTIVE` - `ACTIVE`の逆で、無効状態のとき

### emitter

(文字列) エンチャントが有効な間は、継続中のエミッター識別子。既定値は「エミッターなし」。

### ench_effects

(配列) エンチャントが有効な間、指定された強度で効果を付与します。

単一エントリの構文例:

```json
{
  // (必須) 効果の識別子
  "effect": "effect_identifier",

  // (必須) 強度。強度を1に設定することで、実際に強度が設定されていない効果にも適用できます。
  "intensity": 2
}
```

### hit_you_effect

(配列) エンチャントが有効な間に、キャラクターがモンスターに近接攻撃を行った際に発動する可能性がある呪文のリスト。

単一エントリの構文例:

```json
{
  // (必須) 呪文の識別子
  "id": "spell_identifier",

  // true の場合、呪文はキャラクターの位置に発動します。
  // false の場合、呪文は攻撃しているクリーチャーの位置に発動します。
  // 既定値: false
  "hit_self": false,

  // 発動する確率。X回に1回発動します。
  // 既定値: 1
  "once_in": 1,

  // 呪文が発動したときに表示されるメッセージ(キャラクター用)。
  // %1$s はキャラクター名、%2$s はクリーチャーの名前
  // 既定値: メッセージなし
  "message": "You pierce %2$s with Magic Piercing!",

  // 呪文がNPCに発動したときに表示されるメッセージ。
  // %1$s はNPC名、%2$s はクリーチャーの名前
  // 既定値: メッセージなし
  "npc_message": "%1$s pierces %2$s with Magic Piercing!",

  // TODO: 機能していない？
  "min_level": 1,

  // TODO: 機能していない？
  "max_level": 2
}
```

### hit_me_effect

(配列) エンチャントが有効な間に、キャラクターがクリーチャーから近接攻撃を受けた際に発動する可能性がある呪文のリスト。

`hit_you_effect` と同じ構文です。

### 変異

(配列) エンチャントが有効な間に、一時的に付与される変異のリスト。

### intermittent_activation

(オブジェクト) エンチャントが有効な間に発生するランダム効果を指定するルール。

Syntax:

```json
{
  // エンチャントが有効な間、毎ターン実行されるチェックのリスト。
  "effects": [
    {
      // 平均発動頻度
      // 実際の発動確率は「Xターンに1回発動」となります。
      "frequency": "5 minutes",

      // チェックに成功した場合に発動する呪文のリスト。
      "spell_effects": [
        {
          // (必須) 呪文の識別子
          "id": "nasty_random_effect",

          // TODO: 機能していない？
          "min_level": 1,

          // TODO: 機能していない？
          "max_level": 5
          // TODO: 他のフィールドは読み込まれていますが、使用されていないようです。
        }
      ]
    }
  ]
}
```

### 値 (values)

(配列) キャラクターまたはアイテムの修正対象となるさまざまな値のリスト。

単一エントリの構文例:

```json
{
  // (必須) 修正する値の識別子。以下のリストを参照。
  "value": "VALUE_ID_STRING",

  // 加算ボーナス。省略可能な整数値で、既定値は 0
  // 一部の値では無視される場合があります。
  "add": 13,

  // 乗算ボーナス。省略可能、既定値は 0。
  "multiply": -0.3
}
```

加算ボーナスは乗算ボーナスとは別に適用されます。計算式は次の通りです:

```json
bonus = add + base_value * multiply
```

つまり、`multiply` が -0.8 の場合は -80%、`multiply` が 2.5 の場合は +250% になります。整数値を修正する場合、最終的なボーナスは 0 に向かって切り捨て (四捨五入)されます。

複数のエンチャント(例:アイテムからのエンチャントとバイオニックからのエンチャント)が同じ値を修正する場合、ボーナスは丸められずに合算され、その後合計値が基準値に適用される前に再度丸められます。

キャラクターが同時に持つことのできるエンチャントの数に制限はないため、最終的に計算された値には意図しない挙動を防ぐためのハードコードされた制限が適用されます。

#### 修正可能な値のID

#### キャラクター値

##### 筋力 (STRENGTH)

筋力。この `base_value` は基本の能力値です。最終的な値は 0 以下にはなりません。

##### 器用 (DEXTERITY)

器用 この `base_value` は基本の能力値です。最終的な値は 0 以下にはなりません。

##### 知覚 (PERCEPTION)

知覚 この `base_value` は基本の能力値です。最終的な値は 0 以下にはなりません。

##### 知性 (INTELLIGENCE)

知性 この `base_value` は基本の能力値です。最終的な値は 0 以下にはなりません。

##### 速度 (SPEED)

キャラクターの速度。この `base_value` はキャラクターの速度で、痛み、飢餓、重さのペナルティが含まれます。最終的な速度は基本速度の 25% 以下にはなりません。

##### 攻撃コスト (ATTACK_COST)

近接攻撃のコスト。低いほど良い。この `base_value` は、ステータスやスキルによる修正を含む、装備した武器の攻撃コストです。最終的な値は 25 以下にはなりません。

##### 移動コスト (MOVE_COST)

移動コスト。ここでの `base_value` は、衣服や特性による修正を含むタイルごとの移動コストです。最終的な値は 20 以下にはなりません。

##### 代謝率 (METABOLISM)

代謝率。この修正は `add` フィールドを無視します。この `base_value` は `PLAYER_HUNGER_RATE` を特性によって修正したものです。最終的な値は 0 以下にはなりません。

##### マナ容量 (MANA_CAP)

マナ容量。この `base_value` は、特性によって修正されたキャラクターの基本マナ容量です。最終的な値は 0 以下にはなりません。

##### マナ回復率 (MANA_REGEN)

マナ回復率。この修正は `add` フィールドを無視します。この `base_value` は、特性によって修正されたキャラクターの基本マナ回復率です。最終的な値は 0 以下にはなりません。

##### スタミナ容量 (STAMINA_CAP)

スタミナ容量。この修正は `add` フィールドを無視します。この `base_value` は、特性によって修正されたキャラクターの基本スタミナ容量です。最終的な値は `PLAYER_MAX_STAMINA` の 10% 以下にはなりません。

##### スタミナ回復率 (STAMINA_REGEN)

スタミナ回復率。この修正は `add` フィールドを無視します。この `base_value` は、口の負荷によって修正されたキャラクターの基本スタミナ回復率です。最終的な値は 0 以下にはなりません。

##### 渇き (THIRST)

渇きの回復率。この修正は `add` フィールドを無視します。この `base_value` はキャラクターの基本的な渇き回復率です。最終的な値は 0 以下にはなりません。

##### 疲労 (FATIGUE)

疲労の回復率。この修正は `add` フィールドを無視します。この `base_value` はキャラクターの基本的な疲労回復率です。最終的な値は 0 以下にはなりません。

##### 追加回避 (BONUS_DODGE)

回避ペナルティが発生する前に、1ターンあたり追加される回避回数。この `base_value` は、ペナルティが発生する前のキャラクターの回避回数 (通常は 1)です。最終的な値は 0 以下になることがあります。この場合、回避ロールにペナルティが適用されます。

##### 防御力 (ARMOR_X)

被ダメージの修正。能動防護システムの適用後、防具によってダメージが吸収される前に適用されます。この `base_value` は、対応する属性タイプによって受けるダメージ値です。そのため、`add` が正の値で、`mul` が 1 より大きい場合、キャラクターが受けるダメージが**増加**します。各ダメージタイプにはそれぞれエンチャント値があります。

- `ARMOR_ACID`
- `ARMOR_BASH`
- `ARMOR_BIO`
- `ARMOR_BULLET`
- `ARMOR_COLD`
- `ARMOR_CUT`
- `ARMOR_LIGHT`
- `ARMOR_DARK`
- `ARMOR_PSI`
- `ARMOR_ELEC`
- `ARMOR_HEAT`
- `ARMOR_STAB`

#### アイテム値

##### アイテム攻撃コスト (ITEM_ATTACK_COST)

このアイテムの攻撃コスト(近接または投擲)。状態や場所を無視し、常に有効です。
この `base_value` はアイテムの基本的な攻撃コストです。最終的な値は 0 以下にはなりません。

##### アイテムダメージ (ITEM_DAMAGE_X)

このアイテムの近接ダメージ。状態や場所を無視し、常に有効です。
この `base_value` は、対応する属性タイプの基本ダメージです。最終的な値は 0 以下にはなりません。サポートされているダメージタイプは以下の通りです：

- `ITEM_DAMAGE_BASH`
- `ITEM_DAMAGE_CUT`
- `ITEM_DAMAGE_STAB`
- `ITEM_DAMAGE_BULLET`
- `ITEM_DAMAGE_ACID`
- `ITEM_DAMAGE_BIO`
- `ITEM_DAMAGE_COLD`
- `ITEM_DAMAGE_DARK`
- `ITEM_DAMAGE_ELECTRIC`
- `ITEM_DAMAGE_FIRE`
- `ITEM_DAMAGE_LIGHT`
- `ITEM_DAMAGE_PSI`
- `ITEM_DAMAGE_TRUE`

##### アイテム防御力 (ITEM_ARMOR_X)

このアイテムが受けるダメージの軽減値。アイテムがダメージを吸収する前に適用されます。
この `base_value` は、対応する属性タイプによって受けるダメージ値です。したがって、`add` が正の値で、`mul` が 1 より大きい場合、キャラクターが受けるダメージが**増加**します。各ダメージタイプには、それぞれ固有のエンチャント値があります：

- `ITEM_ARMOR_ACID`
- `ITEM_ARMOR_BASH`
- `ITEM_ARMOR_BIO`
- `ITEM_ARMOR_BULLET`
- `ITEM_ARMOR_COLD`
- `ITEM_ARMOR_CUT`
- `ITEM_ARMOR_LIGHT`
- `ITEM_ARMOR_DARK`
- `ITEM_ARMOR_PSI`
- `ITEM_ARMOR_ELEC`
- `ITEM_ARMOR_HEAT`
- `ITEM_ARMOR_STAB`

## 例

```json
[
  {
    "//": "On-hit effect for ink glands mutation, implemented via enchantment.",
    "type": "enchantment",
    "id": "MEP_INK_GLAND_SPRAY",
    "hit_me_effect": [
      {
        "id": "generic_blinding_spray_1",
        "hit_self": false,
        "once_in": 15,
        "message": "Your ink glands spray some ink into %2$s's eyes.",
        "npc_message": "%1$s's ink glands spay some ink into %2$s's eyes."
      }
    ]
  },
  {
    "//": "This one would look good on a katana for an anime mod.",
    "type": "enchantment",
    "id": "ENCH_ULTIMATE_ASSKICK",
    "has": "WIELD",
    "condition": "ALWAYS",
    "ench_effects": [{ "effect": "invisibility", "intensity": 1 }],
    "hit_you_effect": [{ "id": "AEA_FIREBALL" }],
    "hit_me_effect": [{ "id": "AEA_HEAL" }],
    "mutations": ["KILLER", "PARKOUR"],
    "values": [{ "value": "STRENGTH", "multiply": 1.1, "add": -5 }],
    "intermittent_activation": {
      "effects": [
        {
          "frequency": "1 hour",
          "spell_effects": [
            { "id": "AEA_ADRENALINE" }
          ]
        }
      ]
    }
  }
]
```

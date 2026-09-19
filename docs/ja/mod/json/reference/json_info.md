# JSON INFO

> [!NOTE]
>
> このドキュメントは複数のページに分割されています。

> [!CAUTION]
>
> JSON ファイルの多くはまだ文書化されていない、または古いです。関連するソースファイルを確認してください。
> 確かに。

このドキュメントでは、Cataclysm: Bright Nights で使用される json ファイルの内容を説明します。Cataclysm: Bright Nights のコンテンツを追加または変更し、どこで何を探すべきか、各ファイルやプロパティが何をするのかを知る必要がある場合に読むものです。
何をどこで見つけ、各ファイルとプロパティが何を行うのかについて詳しく学習してください。

## JSON のナビゲート

JSON の多くには、他の JSON エンティティへの相互参照が含まれます。ナビゲートしやすくするために、
`tags` ファイルを構築できるスクリプト `tools/json_tools/cddatags.py` を提供します。

スクリプトを実行するには、Python 3 が必要です。Windows では、おそらくそれをインストールする必要があります。
`.py` ファイルを Python に関連付けます。次に、コマンド プロンプトを開き、Cataclysm: Bright Nights フォルダーに移動して、次のコマンドを実行します。
`tools\json_tools\cddatags.py`。

この機能を使用するには、エディターに [ctags サポート](http://ctags.sourceforge.net/) が必要です。いつ
これは機能しているので、任意のエンティティの定義に簡単にジャンプできるはずです。たとえば、
ID の上にカーソルを置き、適切なキーの組み合わせを押します。

- Vim では、この機能はデフォルトで存在し、次を使用して定義にジャンプできます。
  [`^\]`](http://vimdoc.sourceforge.net/htmldoc/tagsrch.html#tagsrch.txt)。
- Notepad++ で "Plugins" -> "Plugins Admin" に移動し、"TagLEET" プラグインを有効にします。次に、いずれかを選択します
  id を入力し、Alt+Space を押して参照ウィンドウを開きます。

## `type` プロパティ

エントリは、`type` プロパティによって区別されます。このプロパティはすべてのエントリに必須です。
たとえば、このエントリを「armor」に設定すると、ゲームは鎧に固有のプロパティを期待することになります。
そのエントリー。必要に応じて、[`copy-from`](./items/json_inheritance.md#copy-from) とも連携します。
[別のオブジェクトのプロパティを継承](./items/json_inheritance.md)、同じタイプである必要があります。

## フォーマット

JSON ファイルを編集するときは、以下に示すように正しい形式を適用していることを確認してください。

### 期間

数値と期間単位の 1 つ以上のペアを含む文字列。数と単位、および
各ペアは、任意の量のスペースで区切ることができます。利用可能なユニット:

- "hours"、"hour"、"h" - 1 時間
- "days"、"day"、"d" - 1 日
- "minutes"、"minute"、"m" - 1 分
- "turns"、"turn"、"t" - 1 ターン、

例:

- " +1 日 -23 時間 50 分 " `(1*24*60 - 23*60 + 50 == 110 minutes)`
- "1 turn 1 minutes 9 turns" (1ターンは1秒なので1分10秒)

### その他の書式設定

```json
"//" : "comment", // json ファイル内にコメントを残す推奨方法。
```

アイテム名、説明など、一部の json 文字列が翻訳のために抽出されます。
正確な抽出は `lang/extract_json_strings.py` で処理されます。明らかな書き方とは別に
翻訳コンテキストのない文字列ですが、文字列にはオプションの翻訳コンテキストを含めることもできます (および
場合によっては複数形)、次のように記述します。

```json
"name": { "ctxt": "foo", "str": "bar", "str_pl": "baz" }
```

または、複数形が単数形と同じ場合:

```json
"name": { "ctxt": "foo", "str_sp": "foo" }
```

以下のように「//~」エントリを追加することで、翻訳者へのコメントを追加することもできます。の順序は、
エントリーは関係ありません。

```json
"name": {
    "//~": "as in 'foobar'",
    "str": "bar"
}
```

[現在、この構文をサポートしているのは一部の JSON 値のみです](/i18n/reference/translation#supported-json-values)。

## 各 JSON ファイルの説明と内容

このセクションでは、各 json ファイルとその内容について説明します。各 json には独自の固有のプロパティがあります
他の Json ファイルと共有されないもの (たとえば、書籍で使用される 'chapters' プロパティは
防具に適用されます）。これにより、プロパティが のコンテキスト内でのみ記述および使用されるようになります。
適切な JSON ファイル。

## `data/json/` JSON

### アスキーアート

| 識別子  | 説明                                                                                           |
| ------- | ---------------------------------------------------------------------------------------------- |
| id      | 一意のID。連続した 1 つの単語である必要があり、必要に応じてアンダースコアを使用します。        |
| picture | 文字列の配列。各エントリは ASCII ピクチャの 1 行であり、長さは最大 42 列でなければなりません。 |

```json
{
  "type": "ascii_art",
  "id": "cashcard",
  "picture": [
    "",
    "",
    "",
    "       <color_white>╔═══════════════════╗",
    "       <color_white>║                   ║",
    "       <color_white>║</color> <color_yellow>╔═   ╔═╔═╗╔═║ ║</color>   <color_white>║",
    "       <color_white>║</color> <color_yellow>║═ ┼ ║ ║═║╚╗║═║</color>   <color_white>║",
    "       <color_white>║</color> <color_yellow>╚═   ╚═║ ║═╝║ ║</color>   <color_white>║",
    "       <color_white>║                   ║",
    "       <color_white>║   RIVTECH TRUST   ║",
    "       <color_white>║                   ║",
    "       <color_white>║                   ║",
    "       <color_white>║ 555 993 55221 066 ║",
    "       <color_white>╚═══════════════════╝"
  ]
}
```

### ボディパーツ

| 識別子            | 説明                                                                                                                                                                                                                                                                                                                         |
| ----------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id                | (_必須_) 一意の ID。連続した 1 つの単語である必要があり、必要に応じてアンダースコアを使用します。                                                                                                                                                                                                                            |
| name              | (_必須_) ゲーム内名が表示されます。                                                                                                                                                                                                                                                                                          |
| accusative        | (_必須_) この体の部分の対格形。                                                                                                                                                                                                                                                                                              |
| heading           | (_必須_) 見出しに表示される方法。                                                                                                                                                                                                                                                                                            |
| heading_multiple  | (_任意_) 見出しの複数形。 (デフォルト: 見出し値)                                                                                                                                                                                                                                                                             |
| hp_bar_ui_text    | (_オプション_) パネルの HP バーの横にどのように表示されるか。 (デフォルト: 空の文字列)                                                                                                                                                                                                                                       |
| encumbrance_text  | (_任意_) 邪魔された場合の影響の説明。 (デフォルト: 空の文字列)                                                                                                                                                                                                                                                               |
| main_part         | (_任意_) これが付属する主要な部分は何ですか。 (デフォルト: 自分自身)                                                                                                                                                                                                                                                         |
| base_hp           | (_任意_) 変更前のこのパーツの HP 量。 (デフォルト: `60`)                                                                                                                                                                                                                                                                     |
| opposite_part     | (_任意_) ペアの場合、これの反対側の部分は何ですか。 (デフォルト: 自分自身)                                                                                                                                                                                                                                                   |
| essential         | (_任意_) このパーツの HP が `0` まで低下した場合にキャラクターが死亡するかどうか。                                                                                                                                                                                                                                           |
| hit_size          | (_オプション_) フロート。ウェイトなし選択を行うときのボディ部分のサイズ。 (デフォルト: `0.`)                                                                                                                                                                                                                                 |
| hit_size_relative | (_オプション_) フロート。より小さい、同じサイズ、より大きい攻撃者のヒット サイズ。 (デフォルト: `[ 0, 0, 0 ]`                                                                                                                                                                                                                |
| hit_difficulty    | (_オプション_) フロート。 "owner" が攻撃されたと仮定した場合、特定の身体部分を攻撃するのはどのくらい難しいか。数値が大きいほど、良い攻撃がこの部分に向けられることを意味し、数値が小さいほど、この部分が不正確な攻撃を受ける可能性が低いことを意味します。数式は `chance *= pow(hit_roll, hit_difficulty)` (デフォルト: `0`) |
| side              | (_オプション_) この体の部分がどちら側にあるか。両方ともデフォルトです。                                                                                                                                                                                                                                                      |
| stylish_bonus     | (_オプション_) この部分で派手な服を着ると気分ボーナスが得られます。 (デフォルト: `0`)                                                                                                                                                                                                                                        |
| hot_morale_mod    | (_任意_) この部分が熱すぎることによるムード効果。 (デフォルト: `0`)                                                                                                                                                                                                                                                          |
| cold_morale_mod   | (_オプション_) この部分が冷たすぎることによる気分の影響。 (デフォルト: `0`)                                                                                                                                                                                                                                                  |
| bionic_slots      | (_オプション_) このパーツにはバイオニック スロットがいくつありますか。                                                                                                                                                                                                                                                       |

```json
{
  "id": "torso",
  "type": "body_part",
  "name": "torso",
  "accusative": { "ctxt": "bodypart_accusative", "str": "torso" },
  "heading": "Torso",
  "heading_multiple": "Torso",
  "hp_bar_ui_text": "TORSO",
  "encumbrance_text": "Dodging and melee is hampered.",
  "main_part": "torso",
  "opposite_part": "torso",
  "hit_size": 45,
  "hit_size_relative": [20, 33.33, 36.57],
  "hit_difficulty": 1,
  "side": "both",
  "legacy_id": "TORSO",
  "stylish_bonus": 6,
  "hot_morale_mod": 2,
  "cold_morale_mod": 2,
  "base_hp": 60,
  "bionic_slots": 80
}
```

### 夢

| 識別子   | 説明                                                                 |
| -------- | -------------------------------------------------------------------- |
| messages | 潜在的な夢のリスト。                                                 |
| category | 夢を見るために必要な突然変異カテゴリ。                               |
| strength | 変異カテゴリの強度が必要です (1 = 20 ～ 34、2 = 35 ～ 49、3 = 50+)。 |

````json
{
  "messages": [
    "You have a strange dream about birds.",
    "Your dreams give you a strange feathered feeling."
  ],
  "category": "MUTCAT_BIRD",
  "strength": 1
}

### Item Category

When you sort your inventory by category, these are the categories that are displayed.
| Identifier      | Description
|---              |---
| id              | Unique ID. Must be one continuous word, use underscores if necessary
| name            | The name of the category. This is what shows up in-game when you open the inventory.
| zone            | The corresponding loot_zone (see loot_zones.json)
| sort_rank       | Used to sort categories when displaying.  Lower values are shown first
| priority_zones  | When set, items in this category will be sorted to the priority zone if the conditions are met. If the user does not have the priority zone in the zone manager, the items get sorted into zone set in the 'zone' property. It is a list of objects. Each object has 3 properties: ID: The id of a LOOT_ZONE (see LOOT_ZONES.json), filthy: boolean. setting this means filthy items of this category will be sorted to the priority zone, flags: array of flags
|spawn_rate       | Sets amount of items from item category that might spawn.  Checks for `spawn_rate` value for item category.  If `spawn_chance` is less than 1.0, it will make a random roll (0.1-1.0) to check if the item will have a chance to spawn.  If `spawn_chance` is more than or equal to 1.0, it will add a chance to spawn additional items from the same category.  Items will be taken from item group which original item was located in.  Therefore this parameter won't affect chance to spawn additional items for items set to spawn solitary in mapgen (e.g. through use of `item` or `place_item`).

```C++
{
  "id":"armor",
  "name": "ARMOR",
  "zone": "LOOT_ARMOR",
  "sort_rank": -21,
  "priority_zones": [ { "id": "LOOT_FARMOR", "filthy": true, "flags": [ "RAINPROOF" ] } ],
}
````

### 病気

| 識別子             | 説明                                                                                         |
| ------------------ | -------------------------------------------------------------------------------------------- |
| id                 | 一意のID。連続した 1 つの単語である必要があり、必要に応じてアンダースコアを使用します。      |
| min_duration       | 病気が続く可能性がある最小限の期間。文字列 "x m"、"x s"、"x d" を使用します。                |
| max_duration       | 病気が続く最大期間。                                                                         |
| min_intensity      | 病気によって適用される影響の最小強度                                                         |
| max_intensity      | エフェクトの最大強度。                                                                       |
| health_threshold   | これを超えると病気に罹らない健康状態。 -200 ～ 200 の範囲にする必要があります (オプション)。 |
| symptoms           | 病気によって適用される効果。                                                                 |
| affected_bodyparts | エフェクトが適用されるボディパーツのリスト。 (オプション、デフォルトは num_bp)               |

```json
{
  "type": "disease_type",
  "id": "bad_food",
  "min_duration": "6 m",
  "max_duration": "1 h",
  "min_intensity": 1,
  "max_intensity": 1,
  "affected_bodyparts": ["TORSO"],
  "health_threshold": 100,
  "symptoms": "foodpoison"
}
```

### 名前

```json
{ "name" : "Aaliyah", "gender" : "female", "usage" : "given" }, // 名前、性別、"given"/"family"/"city" (姓名/都市名)。
```

### 名前付きの色

```json
{
  "type": "named_color", // ご期待どおり
  "name": "Cataclysm Red", // 表示する名前
  "value": "#622625" // 色の 16 進値
}
```

### 香りの種類

| 識別子            | 説明                                                                                    |
| ----------------- | --------------------------------------------------------------------------------------- |
| id                | 一意のID。連続した 1 つの単語である必要があり、必要に応じてアンダースコアを使用します。 |
| receptive_species | この匂いを追跡できる種。 `species.json` で定義された有効な ID を使用する必要があります  |

```json
{
  "type": "scent_type",
  "id": "sc_flower",
  "receptive_species": ["MAMMAL", "INSECT", "MOLLUSK", "BIRD"]
}
```

### スコアと実績

スコアは、_events_ に基づいて 2 つまたは 3 つのステップで定義されます。どのようなイベントが存在し、どのようなデータが存在するかを確認するには
これらには [`event.h`](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/src/event.h) が含まれています。

各イベントには特定のフィールドのセットが含まれます。各フィールドには文字列キーと `cata_variant` 値があります。
フィールドには、イベントに関するすべての関連情報が提供される必要があります。

たとえば、`gains_skill_level` イベントを考えてみましょう。この仕様は次の場所で確認できます。
`event.h`:

```json
template<>
struct event_spec<event_type::gains_skill_level> {
    static constexpr std::array<std::pair<const char *, cata_variant_type>, 3> fields = {{
            { "character", cata_variant_type::character_id },
            { "skill", cata_variant_type::skill_id },
            { "new_level", cata_variant_type::int_ },
        }
    };
};
```

これから、このイベント タイプには 3 つのフィールドがあることがわかります。

- `character`、レベルを取得するキャラクターの ID。
- `skill`、獲得したスキルの ID。
- `new_level`、そのスキルで新たに取得した整数レベル。

イベントは、ゲーム内の状況に応じてゲームによって生成されます。これらの出来事は変容する可能性があります
と、色々とまとめてあります。イベント ストリーム、イベント統計、イベント ストリームという 3 つの概念が関係しています。
そして得点。

- ゲームによって定義された各 `event_type` はイベント ストリームを生成します。
- 既存のイベント ストリームに `event_transformation` を適用することで、さらにイベント ストリームを json で定義できます。
  イベントストリーム。
- `event_statistic` は、イベント ストリームを単一の値 (通常は数値ですが、その他の値) に要約します。
  値のタイプも可能です)。
- `score` は、そのような統計を使用して、プレイヤーが確認できるゲーム内スコアを定義します。

#### `event_transformation`

`event_transformation` はイベント ストリームを変更して、別のイベント ストリームを生成できます。

変換される入力ストリームは、`"event_type"` として指定されるか、次のいずれかを使用します。
別の JSON 定義の変換されたものを使用するための組み込みイベント タイプ ストリーム、または `"event_transformation"`
イベントストリーム。

次の変更の一部またはすべてをイベント ストリームに加えることができます。

- イベント フィールドの変換に基づいて、各イベントに新しいフィールドを追加します。イベントフィールドの変換
  で見つけることができます
  [`event_field_transformation.cpp`](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/src/event_field_transformations.cpp)。
- イベントに含まれる値に基づいてイベントをフィルタリングし、イベントの一部のサブセットを含むストリームを生成します。
  入力ストリーム。
- 出力ストリームに関係のないいくつかのフィールドを削除します。

各変更の例を次に示します。

```json
"id": "avatar_kills_with_species",
"type": "event_transformation",
"event_type": "character_kills_monster", // 変換はこのタイプのイベントに作用します
"new_fields": { // イベントに追加する新しいフィールドの辞書
    // キーは新しいフィールド名です。値は 1 つの要素の辞書である必要があります
    "species": {
        // キーは適用するevent_field_transformationを指定します。値は指定します
        // その変換に値を提供する必要がある入力フィールド。
        // したがって、この場合、新しいフィールド「種」を追加しています。
        // この殺害事件の犠牲者の種が含まれています。
        "species_of_monster": "victim_type"
    }
}
```

```json
"id": "moves_on_horse",
"type": "event_transformation",
"event_type" : "avatar_moves", // イベントの種類。  変換はこのタイプのイベントに作用します
"value_constraints" : { // 制約の辞書
    // 各キーは制約が適用されるフィールドです
    // 値は制約を指定します。
    // "equals" を使用して、フィールドが取らなければならない定数文字列値を指定できます。
    // "equals_statistic" は、値が何らかの統計の値と一致する必要があることを指定します (下記を参照)。
    "mount" : { "equals": "mon_horse" }
}
// 「mount」が「mon_horse」であるイベントのみをフィルタリングしているため、
// 有用な情報が提供されないため、「マウント」フィールドを削除することもできます。
"drop_fields" : [ "mount" ]
```

#### `event_statistic`

`event_transformation` と同様、`event_statistic` には入力イベント ストリームが必要です。その入力
ストリームは、`event_transformation` と同様に、次の 2 つのいずれかを使用して指定できます。
エントリ:

```json
"event_type" : "avatar_moves" // この組み込みタイプのイベント
"event_transformation" : "moves_on_horse" // この JSON 定義の変換から生じるイベント
```

次に、特定の `stat_type` と、場合によっては追加の詳細を次のように指定します。

イベント数:

```json
"stat_type" : "count"
```

すべてのイベントにわたる指定されたフィールドの数値の合計:

```json
"stat_type" : "total"
"field" : "damage"
```

すべてのイベントにわたる、指定されたフィールドの数値の最大値:

```json
"stat_type" : "maximum"
"field" : "damage"
```

すべてのイベントにわたる、指定されたフィールドの数値の最小値:

```json
"stat_type" : "minimum"
"field" : "damage"
```

考慮すべきイベントが 1 つだけであると仮定し、そのイベントに対して指定されたフィールドの値を取得します。
ユニークなイベント:

```json
"stat_type": "unique_value",
"field": "avatar_id"
```

`stat_type` に関係なく、各 `event_statistic` には次のものも含めることができます。

```json
// スコアと達成要件を説明する際の使用を目的としています。
"description": "Number of things"
```

#### `score`

スコアは、スコアの表の形式を整えるために、説明をイベントに関連付けるだけです。の
`description` は、`%s` 形式指定子を含むことが期待される文字列を指定します。
統計の値が挿入されます。

ほとんどの統計では整数が得られますが、それでも `%s` を使用する必要があることに注意してください。

基礎となる統計に説明がある場合、スコアの説明はオプションです。デフォルトです
「<statistic description>:<value>」に。

```json
"id": "score_headshots",
"type": "score",
"description": "Headshots: %s",
"statistic": "avatar_num_headshots"
```

#### `achievement`

実績とは、一般的に普及している用語の通常の意味で、プレイヤーが目指すべき目標です。
他のゲーム。

実績は要件によって指定され、それぞれが `event_statistic` の制約になります。
たとえば:

```json
{
  "id": "achievement_kill_zombie",
  "type": "achievement",
  // アチーブメントの名前と説明は UI に使用されます。
  // 説明はオプションであり、必要に応じて追加の詳細を提供できます。
  "name": "One down, billions to go\u2026",
  "description": "Kill a zombie",
  "requirements": [
    // 各要件では、制約される統計を指定する必要があります。
    // ある目標値との比較に関する制約。
    { "event_statistic": "num_avatar_zombie_kills", "is": ">=", "target": 1 }
  ]
},
```

`"is"` フィールドは、`">="`、`"<="`、または `"anything"` である必要があります。 `"anything"` ではない場合は `"target"`
存在する必要があり、整数である必要があります。

さらにオプションのフィールドがあります。

```json
"hidden_by": [ "other_achievement_id" ]
```

他の実績 ID のリストを提供します。この実績は非表示になります (つまり、
リストされたすべての実績が完了するまで、実績 UI) を表示します。

これを使用して、ネタバレを防止したり、実績リストの煩雑さを軽減したりできます。

```json
"skill_requirements": [ { "skill": "archery", "is": ">=", "level": 5 } ]
```

これにより、いつ達成できるかについてのスキル レベル要件 (上限または下限) が可能になります。
主張される。 `"skill"` フィールドはスキルの ID を使用します。

以下の `"time_constraint"` のように、統計がリストされている場合にのみ実績を取得できることに注意してください。
`"requirements"` の変更。

```json
"kill_requirements": [ { "faction": "ZOMBIE", "is": ">=", "count": 1 }, { "monster": "mon_sludge_crawler", "is": ">=", "count": 1 } ],
```

これにより、いつアチーブメントを達成できるかについてのキル要件 (上限または下限) が可能になります。
と主張した。 `"faction"` または `"monster"` のいずれかをターゲットとして定義でき、次の種 ID を使用します。
`species.json` または特定のモンスター ID。

エントリごとに `"monster"`/`"faction"` フィールドの 1 つだけを使用できます。どちらも使用しない場合は、いずれか
モンスターは条件を満たします。

現在、NPC をターゲットとして定義できません。

以下の `"time_constraint"` のように、統計がリストされている場合にのみ実績を取得できることに注意してください。
`"requirements"` の変更。

```json
"time_constraint": { "since": "game_start", "is": "<=", "target": "1 minute" }
```

これにより、達成できる時間に時間制限 (下限または上限) を設けることができます。
と主張した。 `"since"` フィールドは、`"game_start"` または `"cataclysm"` のいずれかになります。 `"target"` の説明
その基準点からの経過時間。

実績は、要件にリストされている統計が変更された場合にのみ取得できることに注意してください。
したがって、通常は時間のしきい値に達することでトリガーされる実績が必要な場合は、
（"survived a certain amount of time" など）その場合は、その横に何らかの要件を配置する必要があります。
その時間が経過した後にトリガーします。頻繁に変更される可能性のある統計を選択し、追加します
`"anything"` 制約。たとえば:

```json
{
  "id": "achievement_survive_one_day",
  "type": "achievement",
  "description": "The first day of the rest of their unlives",
  "time_constraint": { "since": "game_start", "is": ">=", "target": "1 day" },
  "requirements": [ { "event_statistic": "num_avatar_wake_ups", "is": "anything" } ]
},
```

これは単純な "survive a day" ですが、目覚めることによってトリガーされるため、
ゲーム開始から24時間後に初めて目覚めます。

### `requirements`

Cataclysm の要件システムは、クラフト、建設、その他のゲーム メカニクスに必要なコンポーネント、ツール、品質の再利用可能なセットを定義します。要件は JSON 形式で定義され、`data/json/requirements/` にあります。

## JSON 構造体

### 基本フォーマット

```json
{
  "id": "unique_id",
  "type": "requirement",
  "//": "Optional comment",
  "components": [
    [
      ["gasoline", 1],
      ["diesel", 1],
      ["biodiesel", 1]
    ]
  ],
  "tools": [
    [
      ["soldering_iron", 1],
      ["soldering_ethanol", 10],
      ["toolset", 1]
    ]
  ],
  "qualities": [
    { "id": "CUT", "level": 1 },
    { "id": "HAMMER", "level": 2 }
  ]
}
```

#### 形式

最後の 3 つの配列は、いずれか 1 つが存在する限りオプションです (結局、要件は _something_ に依存する必要があります!)

**コンポーネント**は、代替案と要件を表すネストされた配列として編成されます。

```json
"components": [
  [ /* Group 1: ONE of these required */ ],
  [ /* Group 2: ONE of these required */ ]
]
```

- `[ "item_id", quantity ]` - 特定のアイテム
- `[ "item_id", quantity, "LIST" ]` - 別の要件定義を参照します

**ツール**は、使用されるが消費されないアイテムと料金を指定します。

```json
"tools": [
  [
    [ "tool_id", charges ],
    [ "alternative", charges, "LIST" ]
  ]
]
```

`charges` は正の整数にすることができ、その場合は消費されたチャージの数を参照します。`-1` はチャージが使用されていないことを示します。

**品質** は、必要な最小限のツール品質レベルを指定します。

````json
"qualities": [
  { "id": "QUALITY_ID", "level": min_level }
]

#### Usage in Recipes

Here is an example of a requirement in a recipe:

```json
"using": [ [ "requirement_id", multiplier ] ]
````

複数の要件:

```json
"using": [
  [ "welding_standard", 1 ],
  [ "forging_standard", 2 ]
]
```

`/data/json/requirements` フォルダー内のファイルを調べて、特定の種類の項目またはコンポーネントの要件の一般的な例を確認できます。要件が循環依存関係を形成していないことを確認してください。

### スキル

```json
"id" : "smg",  // 一意のID。連続した 1 つの単語である必要があり、必要に応じてアンダースコアを使用してください
"name" : "submachine guns",  // ゲーム内で表示される名前
"description" : "Your skill with submachine guns and machine pistols. Halfway between a pistol and an assault rifle, these weapons fire and reload quickly, and may fire in bursts, but they are not very accurate.", // ゲーム内の説明
"tags" : ["gun_type"]  // 特別なフラグ (デフォルト: なし)
```

### ミッション

(オプション、ミッション ID の配列)

この職業/趣味の開始ミッションのリスト。

例:

```JSON
"missions": [ "MISSION_LAST_DELIVERY" ]
```

## `json/` JSON

### 収穫

```json
{
    "id": "jabberwock",
    "type": "harvest",
    "message": "You messily hack apart the colossal mass of fused, rancid flesh, taking note of anything that stands out.",
    "entries": [
      { "drop": "meat_tainted", "type": "flesh", "mass_ratio": 0.33 },
      { "drop": "fat_tainted", "type": "flesh", "mass_ratio": 0.1 },
      { "drop": "jabberwock_heart", "base_num": [ 0, 1 ], "scale_num": [ 0.6, 0.9 ], "max": 3, "type": "flesh" }
    ],
},
{
  "id": "mammal_large_fur",
  "//": "drops large stomach",
  "type": "harvest",
  "entries": [
    { "drop": "meat", "type": "flesh", "mass_ratio": 0.32 },
    { "drop": "meat_scrap", "type": "flesh", "mass_ratio": 0.01 },
    { "drop": "lung", "type": "flesh", "mass_ratio": 0.0035 },
    { "drop": "liver", "type": "offal", "mass_ratio": 0.01 },
    { "drop": "brain", "type": "flesh", "mass_ratio": 0.005 },
    { "drop": "sweetbread", "type": "flesh", "mass_ratio": 0.002 },
    { "drop": "kidney", "type": "offal", "mass_ratio": 0.002 },
    { "drop": "stomach_large", "scale_num": [ 1, 1 ], "max": 1, "type": "offal" },
    { "drop": "bone", "type": "bone", "mass_ratio": 0.15 },
    { "drop": "sinew", "type": "bone", "mass_ratio": 0.00035 },
    { "drop": "fur", "type": "skin", "mass_ratio": 0.02 },
    { "drop": "fat", "type": "flesh", "mass_ratio": 0.07 }
  ]
},
{
  "id": "CBM_SCI",
  "type": "harvest",
  "entries": [
    {
      "drop": "bionics_sci",
      "type": "bionic_group",
      "flags": [ "NO_STERILE", "NO_PACKED" ],
      "faults": [ "fault_bionic_salvaged" ]
    },
    { "drop": "meat_tainted", "type": "flesh", "mass_ratio": 0.25 },
    { "drop": "fat_tainted", "type": "flesh", "mass_ratio": 0.08 },
    { "drop": "bone_tainted", "type": "bone", "mass_ratio": 0.1 }
  ]
},
```

#### `id`

ハーベスト定義の一意の ID。

#### `type`

オブジェクトをハーベスト定義としてマークするには、常に `harvest` にする必要があります。

#### `message`

収穫定義を使用するクリーチャーが屠殺されるときに出力されるオプションのメッセージ。かもしれない
定義から省略されています。

#### `entries`

屠殺時に生産される可能性のあるアイテムとその可能性を定義する辞書の配列
生産された。 `drop` 値は、生成されるアイテムの `id` 文字列である必要があります。

`type` 値は、項目の由来となる関連する本文部分を含む文字列である必要があります。許容可能な値
`flesh`: クリーチャーの "meat"。 `offal`: クリーチャーの "organs"。これらの
現場でのドレッシング時に除去されます。 `skin`: クリーチャーの "skin"。これが台無しになっているものです
四分の一。 `bone`: クリーチャーの "bones"。フィールドからある程度の量を入手できます
ドレッシングをかけて、残りは枝肉を屠殺します。 `bionic`: 解剖によって得られるアイテム
その生き物。 CBM に限定されません。 `bionic_group`: アイテムを与えるアイテムグループ
生き物を解剖すること。 CBM を含むグループに限定されません。

`flags` 値は文字列の配列である必要があります。そのアイテムに追加されるフラグです
収穫時にエントリー。

`faults` 値は `fault_id` 文字列の配列である必要があります。欠点が追加されるのです
収穫時のそのエントリのアイテム。

`bionic` と `bionic_group` 以外のすべての `type` について、次のエントリが結果をスケールします。
`base_num` 値は、最初の要素が最小数を定義する 2 つの要素を含む配列である必要があります。
生産される対応するアイテムの数、および 2 番目の値は最大数を定義します。 `scale_num` 値
2 つの要素を含む配列にする必要があり、それぞれ最小ドロップ数と最大ドロップ数を増やします。
要素値 \* によるサバイバルスキル。 `bas_num` と `scale_num` を計算した後の `max` の上限
`mass_ratio` の値を使用すると、モンスターの重量がどのくらいの量を占めるかの乗数になります。
関連アイテム。質量を節約するには、すべてのドロップの組み合わせを 0 ～ 1 の間に保ちます。これはオーバーライドします
`base_num`、`scale_num`、および `max`

`type` の場合: `bionic` および `bionic_group` 次のエントリにより結果をスケールできます: `max` この値
(他の `type` の `max` とは対照的に) に渡される最大の肉屋ロールに対応します。
activity_handlers.cpp の check_butcher_cbm(); check_butcher_cbm() を表示して、対応するものを確認します
その関数に渡されるロール値の分配確率

### 武器カテゴリ

主に武道で使用される武器 (銃または近接攻撃) をグループに分類するために使用されます。

```json
{
  "type": "weapon_category",
  "id": "WEAP_CAT",
  "name": "Weapon Category"
}
```

`"name"` は武道 UI の UI 表示に使用される翻訳可能な文字列であり、ID は JSON に使用されます。
エントリ。

## 廃止と移行

マップの場合は、スポーンできるすべての場所からアイテムを削除し、mapgen エントリを削除して、
oter_id を移行するために、オーバーマップの地形 ID を `data/json/obsoletion/migration_oter_ids.json` に追加します。
`underground_sub_station` と `sewer_sub_station` を回転可能なバージョンに変換します。mapgen が
この領域はすでに生成されていますが、オーバーマップ上に表示されるタイルのみが変更されます。

```json
{
  "type": "oter_id_migration",
  "//": "obsoleted in 0.4",
  "old_directions": false,
  "new_directions": false,
  "oter_ids": {
    "underground_sub_station": "underground_sub_station_north",
    "sewer_sub_station": "sewer_sub_station_north"
  }
}
```

`old_directions` オプションが有効な場合、各エントリは 4 つの移行を作成します: `old_north` の場合、
`old_west`、`old_south`、および `old_east`。何に移行されるかは、の値によって異なります。
`new_directions` オプション。 `true` に設定されている場合、地形は同じ方向に移行されます。
`old_north` から `new_north`、`old_east` から `new_east` など。 `new_directions` が に設定されている場合
`false` の場合、4 つの地形すべてが 1 つの平坦な `new` に移行されます。どちらの場合も、
`oter_ids` マップには接尾辞を付けずに、プレーンな `old` および `new` 名を指定するだけで済みます。

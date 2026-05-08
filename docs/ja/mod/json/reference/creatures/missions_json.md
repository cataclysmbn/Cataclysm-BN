# ミッションの作成

NPCはプレイヤーにミッションを依頼することができます。ミッションは通常、以下のような構造で定義されます:

```json
{
  "id": "MISSION_GET_BLACK_BOX_TRANSCRIPT",
  "type": "mission_definition",
  "name": "Retrieve Black Box Transcript",
  "description": "Decrypt the contents of the black box using a terminal from a nearby lab.",
  "goal": "MGOAL_FIND_ITEM",
  "difficulty": 2,
  "value": 150000,
  "item": "black_box_transcript",
  "start": {
    "effect": { "u_buy_item": "black_box" },
    "assign_mission_target": { "om_terrain": "lab", "reveal_radius": 3 }
  },
  "origins": ["ORIGIN_SECONDARY"],
  "followup": "MISSION_EXPLORE_SARCOPHAGUS",
  "dialogue": {
    "describe": "With the black box in hand, we need to find a lab.",
    "offer": "Thanks to your searching we've got the black box but now we need to have a look'n-side her.  Now, most buildings don't have power anymore but there are a few that might be of use.  Have you ever seen one of those science labs that have popped up in the middle of nowhere?  Them suckers have a glowing terminal out front so I know they have power somewhere inside'em.  If you can get inside and find a computer lab that still works you ought to be able to find out what's in the black box.",
    "accepted": "Fuck ya, America!",
    "rejected": "Do you have any better ideas?",
    "advice": "When I was play'n with the terminal for the one I ran into it kept asking for an ID card.  Finding one would be the first order of business.",
    "inquire": "How 'bout that black box?",
    "success": "America, fuck ya!  I was in the guard a few years back so I'm confident I can make heads-or-tails of these transmissions.",
    "success_lie": "What?!  I out'ta whip you're ass.",
    "failure": "Damn, I maybe we can find an egg-head to crack the terminal."
  }
}
```

### type

必須項目です。常に "mission_definition" である必要があります。

### id

ミッションIDは必須です。新規ミッションの場合は任意の名前を付けられますが、慣例として "MISSION_" で始め、内容が分かりやすい名前にします。

### name (ミッション名)

必須項目です。ミッションメニュー 'm'キーでプレイヤーに表示される名前です。

### description (ミッションの説明)

必須ではありませんが、ミッションに関する関連情報を要約して記述することを強く推奨します。プレイヤーに実害がない場合に限り、報酬として "u_buy_item" 形式でアイテムを指定して言及することも可能です (以下の例を参照):

```json
"id": "MISSION_EXAMPLE_TOKENS",
"type": "mission_definition",
"name": "Murder Money",
"description": "Whack the target in exchange for <reward_item:FMCNote> c-notes and <reward_item:cig> cigarettes.",
"goal": "MGOAL_ASSASSINATE",
"end": {
  "effect": [
    { "u_buy_item": "FMCNote", "count": 999 },
    { "u_buy_item": "cig", "count": 666 } ]
}
```

このシステムは将来的に、他のミッションパラメータや効果を参照できるよう拡張される可能性があります。

### goal (目的)

必須項目です。以下のいずれかの文字列を指定します:

| goal 文字列               | 達成条件                                                 |
| ------------------------- | -------------------------------------------------------- |
| `MGOAL_GO_TO`             | 指定された広域マップ（Overmap）のタイルに到達する        |
| `MGOAL_GO_TO_TYPE`        | 指定された種類の広域マップタイルのいずれかに到達する     |
| `MGOAL_COMPUTER_TOGGLE`   | 正しい端末を起動すると完了                               |
| `MGOAL_FIND_ITEM`         | 指定された種類のアイテムを1つ以上見つける                |
| `MGOAL_FIND_ITEM_GROUP`   | 指定されたアイテムグループからアイテムを1つ以上見つける  |
| `MGOAL_FIND_ANY_ITEM`     | このミッション用にタグ付けされた特定のアイテムを見つける |
| `MGOAL_FIND_MONSTER`      | 友好的なモンスターを見つけて連れ帰る                     |
| `MGOAL_FIND_NPC`          | 特定のNPCを見つける                                      |
| `MGOAL_TALK_TO_NPC`       | 特定のNPCと話す                                          |
| `MGOAL_RECRUIT_NPC`       | 特定のNPCを仲間に加える                                  |
| `MGOAL_RECRUIT_NPC_CLASS` | 特定のクラスのNPCを仲間に加える                          |
| `MGOAL_ASSASSINATE`       | 特定のNPCを殺害する                                      |
| `MGOAL_KILL_MONSTER`      | 特定の敵対モンスターを殺害する                           |
| `MGOAL_KILL_MONSTER_TYPE` | 特定の種類のモンスターを規定数殺害する                   |
| `MGOAL_KILL_MONSTER_SPEC` | 特定の種族のモンスターを規定数殺害する                   |
| `MGOAL_CONDITION`         | 動的に作成された条件を満たし、依頼主と話す               |

### monster_species

"MGOAL_KILL_MONSTER_SPEC" (特定種族の殺害)を使用する場合に、対象となるモンスターの種族を指定します。

### monster_type

"MGOAL_KILL_MONSTER_TYPE" (特定種類の殺害)を使用する場合に、対象となるモンスターの種類IDを指定します。

### monster_kill_goal

"MGOAL_KILL_MONSTER_SPEC"および"MGOAL_KILL_MONSTER_TYPE"において、ミッション完了までに必要な殺害数を指定します。

### goal_condition

"MGOAL_CONDITION" (特殊条件)を使用する場合に、ミッション完了と判定されるための条件を定義します。条件の記述方法については [NPCs.md](./NPCs)で詳しく解説されており、ミッションでも全く同じ構文を使用します。

### dialogue

NPCがミッションの依頼時や報告時に話すテキストです。ミッションで使われない項目であっても、以下の全てのIDを定義する必要があります。

| 文字列 ID     | 用途                                                     |
| ------------- | -------------------------------------------------------- |
| `describe`    | ミッションの概要説明                                     |
| `offer`       | プレイヤーが検討のためにミッションを選択した際の詳細説明 |
| `accepted`    | プレイヤーがミッションを引き受けた時の反応               |
| `rejected`    | プレイヤーが依頼を断った時の反応                         |
| `advice`      | プレイヤーが攻略のヒントを求めた時の台詞                 |
| `inquire`     | 進行状況を尋ねる時の台詞（NPC側から聞く場合）            |
| `success`     | 成功報告を受けた時の反応                                 |
| `success_lie` | 成功したと嘘をつき、それを見破った時の反応               |
| `failure`     | 失敗報告を受けた時の反応                                 |

### start (開始時処理)

任意項目です。文字列として記述する場合、それは mission_start::内にある関数の名前である必要があります。この関数はミッションポインタを受け取り、開始時のコードを実行します。これにより、標準的なミッションタイプ以外の特殊な挙動をさせることが可能です。※現在"MGOAL_COMPUTER_TOGGLE" (端末操作)を伴うミッションを設定するには、ハードコードされた関数が必要です。

また、文字列の代わりに、以下で説明する「オブジェクト形式」で記述することもできます。

### start / end / fail effects (開始・終了・失敗時のエフェクト)

これらの任意項目が設定されている場合、以下のフィールドを含むオブジェクトとして記述できます:

#### effect

[NPCs.md](./NPCs)で定義されているものと全く同じ形式の「エフェクト配列」です。ここに含まれるすべての値を使用できます。どの場合においても、対象となるNPCは「ミッションの依頼主」となります。

#### reveal_om_ter (地形の開示)

広域マップ (Overmap)の地形IDを、文字列または文字列のリストで指定します。指定されたID (リストの場合はその中からランダムに1つ) に一致する広域マップ上のタイルが1ヵ所探索済みになり、さらに3分の1の確率で、その地点までの道路ルートも開示されます。

#### assign_mission_target (ミッション目標地点の指定)

`assign_mission_target` オブジェクトは、特定の地形を探索 (あるいは必要に応じて生成)し、それをミッションの「目標地点」として指定するための条件を定義します。このパラメータによって、地点の選定方法や、選定後のエフェクト（周辺エリアの視界確保など）を制御できます。`om_terrain` フィールドのみが必須項目です。

| 識別子                                     | 説明                                                                                                    |
| ------------------------------------------ | ------------------------------------------------------------------------------------------------------- |
| `om_terrain`                               | 目標として選択される広域マップ（Overmap）地形のID。必須項目です。                                       |
| `om_terrain_match_type`                    | `om_terrain`の照合ルールを指定します。デフォルトは TYPE です。詳細は後述します。                        |
| `om_special`                               | 対象の地形が含まれる「広域マップスペシャル(Overmap Special)」のID。                                     |
| `om_terrain_replace`                       | `om_terrain` が見つからなかった場合に探索・置換の対象となる地形のID。                                   |
| `reveal_radius`                            | 地図上で視界を確保する (霧を晴らす) 半径(広域マップ座標単位)。                                          |
| `must_see`                                 | trueの場合、すでに探索済みの地形からのみ `om_terrain` を探します。                                      |
| `cant_see`                                 | trueの場合、まだ探索されていない地形からのみ `om_terrain` を探します。                                  |
| `random`                                   | trueの場合、条件に合う地形`om_terrain`からランダムに選択します。falseの場合は最も近い地点を選択します。 |
| `search_range`                             | `om_terrain`を探索する範囲 (広域マップ座標単位)。                                                       |
| `min_distance`                             | 指定した範囲内にある `om_terrain` を無視します(近すぎる場所を避ける際に使用)。                          |
| `origin_npc`                               | プレイヤーの現在位置ではなく、NPCの現在位置を中心に探索を開始します。                                   |
| `z`                                        | 指定した場合、プレイヤーやNPCの階層に関わらず、このZレベル（階層）を探索します。                        |
| `offset_x`,<br\>`offset_y`,<br\>`offset_z` | `om_terrain`地形を発見・生成した後、ミッション目標地点を指定した座標分だけオフセット(移動)させます。    |

**example**

```json
{
  "assign_mission_target": {
    "om_terrain": "necropolis_c_44",
    "om_special": "Necropolis",
    "reveal_radius": 1,
    "must_see": false,
    "random": true,
    "search_range": 180,
    "z": -2
  }
}
```

もし `om_terrain` が「広域マップスペシャル（Overmap Special）」の一部であるなら、 `om_special` の値も併せて指定することが不可欠です。そうしないと、ゲーム側がそのスペシャル全体をどのように生成すればよいか判断できなくなります。

`om_terrain_match_type` は、未指定の場合はデフォルトで TYPEに設定されます。以下の値を指定可能です:

- `EXACT` - 指定した文字列が、広域マップ地形IDと完全に一致する必要があります。これには、直線状の地形を示す方向サ
  フィックスや、回転済みの地形を示す回転サフィックスも含まれます。

- `TYPE` - 指定した文字列が、広域マップ地形IDの基本タイプIDと完全に一致する必要があります。つまり、回転や直線地形
  に関するサフィックスは無視されます。

- `PREFIX` - 指定した文字列が、広域マップ地形IDの完全な接頭辞（アンダースコアで区切られた最初の部分）である必要が
  あります。例えば、"forest" は "forest" や "forest_thick" には一致しますが、"forestcabin" には一致しません。

- `CONTAINS` - 指定した文字列が広域マップ地形IDの中に含まれている必要があります。位置(先頭・中間・末尾) は問わ
  ず、アンダースコアの区切りルールも適用されません。

`om_special` を新たに配置する必要がある場合、そのスペシャルの定義で定められた配置ルール (配置可能な地形、都市からの距離、道路接続など)に従います。そのため、配置ルールが厳格であればあるほど、配置に失敗する可能性が高くなります(すでに生成済みの他のスペシャルとの場所の取り合いになるためです)。

`om_terrain_replace` は、`om_terrain`が広域マップスペシャルの一部ではない場合にのみ関係します。この値は `om_terrain`が見つからなかった場合に使用され、代替のターゲットとして探し出された後、その場所が `om_terrain` の値に置き換えられます。

`must_see` を true に設定した場合、対象の地形が見つからなくても、ゲーム側で新しく地形を生成することはありません。これは、プレイヤーがすでに探索したエリアに新しい地形が魔法のように突然現れるのを避けるための仕様です。

`reveal_radius` (視界確保半径)、`min_distance` (最小距離)、`search_range` (探索範囲)はすべて広域マップ座標(OMT：Overmap Terrain単位)で指定します(現在、1つの広域マップは 180x180 OMTユニットで構成されています)。探索は通常プレイヤーを中心に計算されますが、`origin_npc`が設定されている場合はNPCを中心とします。現状、この両者に差が出ることは稀ですが、無線越しにミッションを受注する場合などに影響します。探索処理は既存の広域マップを優先し（既存マップで見つからない場合のみ、新しいマップを生成します）、プレイヤーと同じZレベル(階層)でのみ実行されます。`z` 属性を使用すると、これを上書きしてプレイヤーとは別の階層を探索できます。この値は相対値ではなく絶対値です。

`offset_x`、`offset_y`、`offset_z` は、ミッション目標の最終的な位置を指定した値だけ移動させます。これにより、ミッションの目的地が`om_terrain`で指定した地形そのものではなくなる (隣のマスにずれる等) 場合があります。

#### update_mapgen (マップ生成情報の更新)

`update_mapgen` オブジェクト(または配列)を使用すると、既存の広域マップタイル("assign_mission_target"で作成されたものを含む) を修正して、ミッション固有のモンスター、NPC、コンピュータ、アイテムなどを追加できます。

配列として使用する場合: `update_mapgen` は2つ以上の `update_mapgen`オブジェクトで構成されます。

オブジェクトとして使用する場合: 任意の有効なJSON形式の「mapgenオブジェクト」を記述できます。これらの要素は、"assign_mission_target"で指定された目標地点、あるいは`om_terrain` and `om_special` フィールドで指定された最も近い広域マップ地形に配置されます。もし`mapgen_update_id`が指定されている場合は、一致するIDを持つ `mapgen_update` オブジェクトが実行されます。

JSON形式のmapgenおよび`update_mapgen`に関する詳細は、`doc/MAPGEN.md`を参照してください。

`update_mapgen`を使って配置されたNPC、モンスター、コンピュータをミッションの「直接のターゲット」にするには、
`update_mapgen`内の`place`オブジェクトにある`target`フラグ (bool値)を true に設定してください。

## Adding new missions to NPC dialogue (NPCの会話に新しいミッションを追加する)

NPCにミッションを割り当てるための第一歩は、そのNPCの定義を見つけることです。ユニークNPCの場合、通常はNPC用JSONファイルの冒頭に記述されており、以下のようになっています:

```json
{
  "type": "npc",
  "id": "refugee_beggar2",
  "//": "Schizophrenic beggar in the refugee center.",
  "name_unique": "Dino Dave",
  "gender": "male",
  "name_suffix": "beggar",
  "class": "NC_BEGGAR_2",
  "attitude": 0,
  "mission": 7,
  "chat": "TALK_REFUGEE_BEGGAR_2",
  "faction": "lobby_beggars"
},
```

ここに、NPCが最初に提示するミッションを定義する行を追加します。 例:

```
"mission_offered": "MISSION_BEGGAR_2_BOX_SMALL"
```

ミッションを持つNPCは、プレイヤーが最初のミッションを開始できるように、TALK_MISSION_LISTへ繋がる会話選択肢を持っている必要があります。さらに、以下のいずれかの設定を行ってください。

- data/json/npcs/TALK_COMMON_MISSION.json の最初の talk_topicにある「汎用ミッション応答IDリスト」に、そのNPCの
  talk_topic IDを追加する。
- あるいは、同様の talk_topic を自作し、TALK_MISSION_INQUIRE (進行状況の確認) や
  TALK_MISSION_LIST_ASSIGNED (受注済みミッション一覧) へ繋がる応答を設定する。

これらのいずれかを行うことで、プレイヤーはそのNPCと通常のミッション管理に関する会話ができるようになります。

以下は、カスタムミッションの問い合わせがどのように表示されるかの例です。これはプレイヤーがすでにミッションを受注している場合にのみ、NPCの会話肢に表示されます。

```json
{
    "type": "talk_topic",
    "//": "Generic responses for Old Guard Necropolis NPCs that can have missions",
    "id": [ "TALK_OLD_GUARD_NEC_CPT", "TALK_OLD_GUARD_NEC_COMMO" ],
    "responses": [
      {
        "text": "About the mission...",
        "topic": "TALK_MISSION_INQUIRE",
        "condition": { "and": [ "has_assigned_mission", { "u_is_wearing": "badge_marshal" } ] }
      },
      {
        "text": "About one of those missions...",
        "topic": "TALK_MISSION_LIST_ASSIGNED",
        "condition": { "and": [ "has_many_assigned_missions", { "u_is_wearing": "badge_marshal" } ] }
      }
    ]
},
```
